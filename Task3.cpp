// Task 3: System Programming / File I/O & Processes
// Demonstrates: process creation (fork), inter-process communication (pipe),
// and file handling, with graceful error handling.
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cerrno>
#include <fstream>
using namespace std;

const int NUM_ITEMS = 5;

int main() {
    int pipefd[2]; // pipefd[0] = read end, pipefd[1] = write end

    // ---- Create the pipe (IPC channel) ----
    if (pipe(pipefd) == -1) {
        cerr << "Error: failed to create pipe - " << strerror(errno) << "\n";
        return 1;
    }

    // ---- Create the child process ----
    pid_t pid = fork();

    if (pid < 0) {
        // Edge case: fork failed (e.g. system resource limit reached)
        cerr << "Error: fork() failed - " << strerror(errno) << "\n";
        return 1;
    }

    if (pid == 0) {
        // ---------------- CHILD PROCESS = PRODUCER ----------------
        close(pipefd[0]); // producer doesn't read, close read end

        for (int i = 1; i <= NUM_ITEMS; i++) {
            string message = "item-" + to_string(i);

            // Edge case: handle a failed/partial write
            ssize_t written = write(pipefd[1], message.c_str(), message.size());
            if (written == -1) {
                cerr << "Producer error: write failed - " << strerror(errno) << "\n";
                close(pipefd[1]);
                return 1;
            }

            cout << "[Producer PID " << getpid() << "] sent: " << message << "\n";
            usleep(200000); // simulate work, 0.2s delay
        }

        close(pipefd[1]); // done writing, close write end so consumer sees EOF
        return 0;

    } else {
        // ---------------- PARENT PROCESS = CONSUMER ----------------
        close(pipefd[1]); // consumer doesn't write, close write end

        char buffer[64];
        ssize_t bytesRead;

        // Also log everything received to a file (file handling requirement)
        ofstream logFile("received_log.txt");
        if (!logFile.is_open()) {
            cerr << "Error: could not open log file for writing\n";
        }

        cout << "\n[Consumer PID " << getpid() << "] waiting for items...\n";

        // Edge case: read() returns 0 when pipe is closed (EOF) -- loop ends cleanly
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            cout << "[Consumer] received: " << buffer << "\n";
            if (logFile.is_open()) {
                logFile << buffer << "\n";
            }
        }

        if (bytesRead == -1) {
            // Edge case: read() itself failed (not just EOF)
            cerr << "Consumer error: read failed - " << strerror(errno) << "\n";
        } else {
            cout << "[Consumer] pipe closed by producer, no more data (EOF).\n";
        }

        if (logFile.is_open()) logFile.close();
        close(pipefd[0]);

        // ---- Wait for child to finish, avoid zombie process ----
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            cout << "[Consumer] producer exited normally, code "
                 << WEXITSTATUS(status) << "\n";
        } else {
            cout << "[Consumer] producer terminated abnormally.\n";
        }

        cout << "Log written to received_log.txt\n";
    }

    return 0;
}
