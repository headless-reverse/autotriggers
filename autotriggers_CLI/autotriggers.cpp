#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <libudev.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
std::atomic<bool> monitoring_running(false);

// Logger
class KernelLogger {
public:
    KernelLogger(const std::string& path = "/var/log/autotriggers.log") : log_path(path) {}
    void log(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        std::ofstream f(log_path, std::ios::app);
        if (f.is_open()) {
            f << timestamp() << " " << msg << std::endl;
        }
    }
private:
    std::string log_path;
    std::mutex mutex;
    std::string timestamp() {
        std::time_t now = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", std::localtime(&now));
        return std::string(buf);
    }
};

// Trigger structure
struct TriggerRule {
    std::string script;
    std::vector<std::string> args;
    bool auth_required = false;
    int delay_sec = 0;
};

// Load triggers from JSON
std::map<std::string, std::vector<TriggerRule>> loadTriggers(const std::string& config_file) {
    std::map<std::string, std::vector<TriggerRule>> triggers;
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "[!] Nie mozna otworzyc '" << config_file << "'." << std::endl;
        return triggers;
    }

    try {
        json j = json::parse(file);
        for (auto& [vid_pid, actions] : j.items()) {
            std::vector<TriggerRule> rules;
            for (const auto& action : actions) {
                TriggerRule rule;
                rule.script = action.value("action_script", "");
                rule.auth_required = action.value("auth_required", false);
                rule.delay_sec = action.value("delay_sec", 0);
                if (action.contains("action_args") && action["action_args"].is_array()) {
                    for (const auto& arg : action["action_args"]) {
                        rule.args.push_back(arg.get<std::string>());
                    }
                }
                rules.push_back(rule);
            }
            triggers[vid_pid] = rules;
        }
        std::cout << "[✓] Wczytano konfiguracje z '" << config_file << "'." << std::endl;
    } catch (json::parse_error& e) {
        std::cerr << "[!] Blad parsowania: " << e.what() << std::endl;
    }

    return triggers;
}

// Save triggers to JSON
void saveTriggers(const std::string& config_file, const std::map<std::string, std::vector<TriggerRule>>& triggers) {
    json j;
    for (const auto& [vid_pid, rules] : triggers) {
        json actions_array = json::array();
        for (const auto& rule : rules) {
            json action;
            action["action_script"] = rule.script;
            action["action_args"] = rule.args;
            action["auth_required"] = rule.auth_required;
            action["delay_sec"] = rule.delay_sec;
            actions_array.push_back(action);
        }
        j[vid_pid] = actions_array;
    }

    std::ofstream file(config_file);
    if (file.is_open()) {
        file << std::setw(4) << j << std::endl;
        std::cout << "[✓] Zapisano konfiguracje do '" << config_file << "'." << std::endl;
    } else {
        std::cerr << "[!] Blad zapisu do '" << config_file << "'." << std::endl;
    }
}

// Execute script
void executeScriptWithDelay(const TriggerRule& rule, KernelLogger& logger) {
    if (access(rule.script.c_str(), X_OK) != 0) {
        logger.log("[X] Skrypt '" + rule.script + "' nie jest wykonalny.");
        return;
    }

    if (rule.delay_sec > 0) {
        logger.log("[•] Opóźnienie " + std::to_string(rule.delay_sec) + "s dla '" + rule.script + "'");
        std::this_thread::sleep_for(std::chrono::seconds(rule.delay_sec));
    }

    pid_t pid = fork();
    if (pid == -1) {
        logger.log("[!] fork() nie powiodlo sie.");
        return;
    }

    if (pid == 0) {
        std::vector<char*> argv_vec;
        argv_vec.push_back(const_cast<char*>(rule.script.c_str()));
        for (const auto& arg : rule.args) {
            argv_vec.push_back(const_cast<char*>(arg.c_str()));
        }
        argv_vec.push_back(nullptr);

        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        execvp(rule.script.c_str(), argv_vec.data());
        perror("execvp");
        exit(1);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            logger.log("[✓] Akcja '" + rule.script + "' zakonczona sukcesem.");
        } else {
            logger.log("[X] Akcja '" + rule.script + "' blad. Kod: " + std::to_string(WEXITSTATUS(status)));
        }
    }
}

// Monitor USB
void monitorUsbEvents(const std::string& config_file) {
    KernelLogger logger;
    std::cout << "[•] Monitoring zdarzen USB z '" << config_file << "'." << std::endl;

    struct udev* udev = udev_new();
    if (!udev) {
        logger.log("[!] Nie mozna utworzyc kontekstu udev.");
        return;
    }

    struct udev_monitor* mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!mon) {
        logger.log("[!] Nie mozna utworzyc monitora udev.");
        udev_unref(udev);
        return;
    }

    udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device");
    udev_monitor_enable_receiving(mon);
    int fd = udev_monitor_get_fd(mon);

    while (monitoring_running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {0, 100000};
        select(fd + 1, &fds, NULL, NULL, &tv);

        if (FD_ISSET(fd, &fds)) {
            struct udev_device* dev = udev_monitor_receive_device(mon);
            if (dev) {
                const char* action = udev_device_get_action(dev);
                if (action && std::string(action) == "add") {
                    const char* vendor_id = udev_device_get_sysattr_value(dev, "idVendor");
                    const char* product_id = udev_device_get_sysattr_value(dev, "idProduct");
                    if (vendor_id && product_id) {
                        std::string vid_pid = std::string(vendor_id) + ":" + std::string(product_id);
                        const char* product = udev_device_get_sysattr_value(dev, "product");
                        const char* manufacturer = udev_device_get_sysattr_value(dev, "manufacturer");
                        std::string device_name = (manufacturer && product) ?
                            std::string(manufacturer) + " " + std::string(product) :
                            "nieznane urzadzenie";

                        std::cout << "\n[+] Wykryto: " << device_name << " (" << vid_pid << ")" << std::endl;
                        auto triggers = loadTriggers(config_file);
                        if (triggers.count(vid_pid)) {
                            std::cout << "  [•] Akcje: " << triggers[vid_pid].size() << std::endl;
                            for (const auto& rule : triggers[vid_pid]) {
                                executeScriptWithDelay(rule, logger);
                            }
                        } else {
                            std::cout << "  [•] Brak akcji dla " << vid_pid << "." << std::endl;
                        }
                    }
                }
                udev_device_unref(dev);
            }
        }
    }

    udev_monitor_unref(mon);
    udev_unref(udev);
}

// CLI usage
void usage(const std::string& name) {
    std::cout << "Uzycie: " << name << " [--config <plik>] [--daemon] [--help]" << std::endl;
}

// Main
int main(int argc, char* argv[]) {
    if (getuid() != 0) {
        std::cerr << "[!] you are not root." << std::endl;
        return 1;
    }

    std::string config_file = "triggers.json";
    bool run_as_daemon = false;
    bool show_help = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--daemon") {
            run_as_daemon = true;
        } else if (arg == "--help") {
            show_help = true;
        } else {
            std::cerr << "[!] Nieznana opcja: " << arg << std::endl;
            usage(argv[0]);
            return 1;
        }
    }

    if (show_help) {
        usage(argv[0]);
        return 0;
    }

    if (run_as_daemon) {
        monitoring_running = true;
        monitorUsbEvents(config_file);
    } else {
        std::map<std::string, std::vector<TriggerRule>> triggers = loadTriggers(config_file);
        std::string choice;
        while (true) {
            std::cout << "\n### Autotriggers Menu ###" << std::endl;
            std::cout << "1. Pokaz konfiguracje" << std::endl;
            std::cout << "2. Dodaj trigger" << std::endl;
            std::cout << "3. Usun trigger" << std::endl;
            std::cout << "4. Start monitoring" << std::endl;
            std::cout << "5. Stop monitoring" << std::endl;
            std::cout << "6. Zapisz konfiguracje" << std::endl;
            std::cout << "7. Wyjdz" << std::endl;
            std::cout << "Wybierz opcje: ";
            std::cin >> choice;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            if (choice == "1") {
                for (const auto& [vid_pid, rules] : triggers) {
                    std::cout << "\n[" << vid_pid << "]" << std::endl;
                    for (size_t i = 0; i < rules.size(); ++i) {
                        const auto& rule = rules[i];
                        std::cout << "  #" << i + 1 << ": " << rule.script << " ";
                        for (const auto& arg : rule.args) std::cout << arg << " ";
                        std::cout << "| auth: " << (rule.auth_required ? "Tak" : "Nie");
                        std::cout << " | delay: " << rule.delay_sec << "s" << std::endl;
                    }
                }
            } else if (choice == "2") {
                std::string vid_pid;
                std::cout << "VID:PID: ";
                std::cin >> vid_pid;
                std::cin.ignore();

                TriggerRule rule;
                std::cout << "Sciezka do skryptu: ";
                std::getline(std::cin, rule.script);

                std::cout << "Argumenty (spacja): ";
                std::string args_line;
                std::getline(std::cin, args_line);
                std::stringstream ss(args_line);
                std::string arg;
                while (ss >> arg) rule.args.push_back(arg);

                std::cout << "Wymaga autoryzacji (tak/nie): ";
                std::string auth;
                std::cin >> auth;
                rule.auth_required = (auth == "tak");

                std::cout << "Opóźnienie (s): ";
                std::cin >> rule.delay_sec;

                triggers[vid_pid].push_back(rule);
                std::cout << "[✓] Dodano trigger dla " << vid_pid << std::endl;
            } else if (choice == "3") {
                std::string vid_pid;
                std::cout << "VID:PID do usuniecia: ";
                std::cin >> vid_pid;
                if (triggers.erase(vid_pid)) {
                    std::cout << "[✓] Usunieto trigger dla " << vid_pid << std::endl;
                } else {
                    std::cout << "[!] Nie znaleziono " << vid_pid << std::endl;
                }
            } else if (choice == "4") {
                if (!monitoring_running) {
                    monitoring_running = true;
                    std::thread monitor_thread(monitorUsbEvents, config_file);
                    monitor_thread.detach();
                    std::cout << "[✓] Monitoring uruchomiony." << std::endl;
                } else {
                    std::cout << "[!] Monitoring juz dziala." << std::endl;
                }
            } else if (choice == "5") {
                if (monitoring_running) {
                    monitoring_running = false;
                    std::cout << "[✓] Monitoring zatrzymany." << std::endl;
                } else {
                    std::cout << "[!] Monitoring nie byl aktywny." << std::endl;
                }
            } else if (choice == "6") {
                saveTriggers(config_file, triggers);
            } else if (choice == "7") {
                std::cout << "[✓] Zakonczono." << std::endl;
                break;
            } else {
                std::cout << "[!] Nieprawidlowa opcja." << std::endl;
            }
        }
    }

    return 0;
}
