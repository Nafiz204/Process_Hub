# 🚀 Process Manager (PM)

A high-performance, modular process management suite for Linux, featuring both a powerful command-line interface and a modern GTK3-based graphical user interface.

![C](https://img.shields.io/badge/Language-C-A8B9CC?style=for-the-badge&logo=c)
![Linux](https://img.shields.io/badge/Platform-Linux-FCC624?style=for-the-badge&logo=linux)
![GTK3](https://img.shields.io/badge/UI-GTK3-7FE719?style=for-the-badge&logo=gtk)
![License](https://img.shields.io/badge/License-MIT-blue.svg?style=for-the-badge)

## ✨ Features

- **📊 Real-time Monitoring:** Advanced task manager for tracking system processes.
- **🛠️ Job Control:** Full support for foreground (`fg`) and background (`bg`) job management.
- **📡 Signal Handling:** Seamlessly send signals (SIGKILL, SIGSTOP, SIGCONT) to manage process states.
- **💻 Integrated Shell:** Custom shell mode for executing external commands and scripts.
- **🎨 Hybrid Interface:** Choose between a sleek CLI (`pm_shell`) or a native GTK3 GUI (`pm_gui`).

---

## 🛠️ Installation & Build

### Prerequisites
Ensure you have the following installed on your Linux system:
- `gcc` (GNU Compiler Collection)
- `libgtk-3-dev` (For the GUI version)

### Building the Project
Clone the repository and run the provided Makefile:

```bash
git clone https://github.com/yourusername/process-manager.git
cd process-manager
make all
```

This will generate two binaries:
- `pm_shell`: The interactive CLI version.
- `pm_gui`: The graphical GTK3 version.

---

## 🚀 Usage

### Command Line Interface
Launch the shell version for a classic terminal experience:
```bash
./pm_shell
```
**Menu Options:**
1. **Monitor Processes:** View active processes.
2. **Job Management:** Switch between foreground and background jobs.
3. **Process Control:** Send signals to PIDs.
4. **PM Shell Mode:** Use it as a functional system shell.

### Graphical User Interface
Launch the GUI for a more visual experience:
```bash
./pm_gui
```
The GUI provides a clean, list-based view of processes with interactive controls powered by GTK3.

---

## 📂 Project Structure

| File | Description |
| :--- | :--- |
| `main.c` | Entry point for the CLI application. |
| `gui_main.c` | Entry point and layout for the GTK3 GUI. |
| `process_mgmt.c` | Core logic for process execution and monitoring. |
| `job_control.c` | Handles foreground/background job states. |
| `signals.c` | Manages UNIX signal handling and process communication. |
| `utils.c` | Shared helper functions and formatting. |

---

## 📜 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing
Contributions are welcome! Please feel free to submit a Pull Request.
