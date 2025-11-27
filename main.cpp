#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>

using namespace std;

// ============================
// Задание 9 — обработка SIGHUP
// ============================
void handle_sighup(int) {
    cout << "Configuration reloaded\n";
}

// ============================
// История команд (Задание 4)
// ============================
string history_path = string(getenv("HOME")) + "/.kubsh_history";

void save_history(const vector<string>& history) {
    ofstream f(history_path);
    for (auto& h : history) f << h << "\n";
}

// ============================
// Запуск бинарников (Задание 8)
// ============================
bool run_binary(const string& cmd, vector<string> args) {
    string path = getenv("PATH");
    stringstream ss(path);
    string dir;

    while (getline(ss, dir, ':')) {
        string full = dir + "/" + cmd;
        if (access(full.c_str(), X_OK) == 0) {
            vector<char*> cargs;
            cargs.push_back(const_cast<char*>(full.c_str()));
            for (auto& a : args) cargs.push_back(const_cast<char*>(a.c_str()));
            cargs.push_back(nullptr);

            if (fork() == 0) {
                execv(full.c_str(), cargs.data());
                exit(1);
            } else {
                wait(nullptr);
                return true;
            }
        }
    }
    return false;
}

// ============================
// Задание 10 — инфо о разделе
// ============================
void list_partitions(const string& device) {
    string cmd = "lsblk " + device + " -o NAME,SIZE,TYPE,MOUNTPOINT";
    system(cmd.c_str());
}

// ============================
// Шелл (Задания 1–7)
// ============================
int main() {
    signal(SIGHUP, handle_sighup);

    vector<string> history;

    cout << "kubsh started\n";

    string line;

    while (true) {
        cout << "> ";
        if (!getline(cin, line)) break; // Задание 2 — выход по Ctrl+D

        history.push_back(line);

        // Задание 3 — выход по \q
        if (line == "\\q") break;

        // Задание 5 — echo
        if (line.rfind("echo ", 0) == 0) {
            cout << line.substr(5) << "\n";
            continue;
        }

        // Задание 7 — вывод переменной окружения
        if (line.rfind("\\e ", 0) == 0) {
            string var = line.substr(3);
            if (var[0] == '$') var = var.substr(1);
            char* v = getenv(var.c_str());
            if (v) {
                string val = v;
                stringstream ss(val);
                string part;
                while (getline(ss, part, ':'))
                    cout << part << "\n";
            }
            continue;
        }

        // Задание 10 — инфо о дисках
        if (line.rfind("\\l ", 0) == 0) {
            list_partitions(line.substr(3));
            continue;
        }

        // Ввод пустой строки — пропуск
        if (line.empty()) continue;

        // Задание 1 — просто печать строки
        cout << line << "\n";

        // Задание 6 — проверка команды + выполнение
        string cmd;
        string arg;
        stringstream ss(line);
        ss >> cmd;
        vector<string> args;
        while (ss >> arg) args.push_back(arg);

        run_binary(cmd, args);
    }

    save_history(history);

    return 0;
}
