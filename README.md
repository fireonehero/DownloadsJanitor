# 🧹 DownloadsJanitor

**DownloadsJanitor** is an automated, rules-based file organizer for Windows. Built in modern C++, this tool monitors specified folders (like your 'Downloads') and automatically moves files to their correct destinations based on your custom JSON rules, keeping your system clean and tidy.

> **Note:** DownloadsJanitor targets Windows 10/11. The current build uses the Win32 API for startup registration and file change notifications.

---

### ✨ Features

* **Rules-Based Organizing:** Define simple or complex rules in an easy-to-edit `rules.json` file.
* **Automatic Sorting:** Sorts files based on their extensions (e.g., `.png`, `.jpg` -> `Pictures/`).
* **Automatic Folder Creation:** If a destination folder doesn't exist, DownloadsJanitor will create it for you.
* **Modern C++:** Built using C++17 for high-performance I/O.
* **Background Monitoring:** Relies on Windows change notifications to react as soon as new files appear.

---

### 🚀 Getting Started

To get a local copy up and running, follow these simple steps.

#### Prerequisites (Windows)

* A C++17 compatible compiler (MSVC, GCC, or Clang)
* [CMake](https://cmake.org/download/) 3.16+
* [Git](https://git-scm.com/downloads)

#### Building the Project

1.  **Clone the repo:**
    ```sh
    git clone [https://github.com/your-username/DownloadsJanitor.git](https://github.com/your-username/DownloadsJanitor.git)
    cd DownloadsJanitor
    ```

2.  **Configure and build (Windows example using Ninja):**
    ```powershell
    cmake -S . -B build-win -G "Ninja"
    cmake --build build-win --config Release
    ```
    *(Use `-G "Visual Studio 17 2022" -A x64` if you prefer MSBuild.)*

3.  **Copy the config folder alongside the executable (first run only):**
    ```powershell
    robocopy config build-win\config /mir
    ```

4.  **Run the application:**
    ```powershell
    .\build-win\DownloadsJanitor.exe
    ```

    On startup it registers itself under the current user’s Run key and processes the watch folder once, then keeps watching for changes.

#### Launching without a console window

If you prefer the background watcher to start silently, use the helper script:

Just double-click `scripts\RunDownloadsJanitorHidden.vbs` (or run the command below) and it will locate `build-win\DownloadsJanitor.exe`, check that `config\` is present beside it, and launch silently:

```powershell
wscript.exe ".\scripts\RunDownloadsJanitorHidden.vbs"
```

---

### ⚙️ Configuration (`config/rules.json`)

The application reads `config/rules.json` from its working directory. The structure supports built-in defaults, reusable placeholders, and custom rules:

```json
{
  "user": "YourWindowsUser",
  "watch_folder": "C:/Users/{{user}}/Downloads",
  "use_default_rules": true,
  "default_rules": [
    {
      "extensions": [".exe", ".msi", ".jar"],
      "destination": "C:/Installers"
    }
    // … more defaults
  ],
  "custom_rules": [
    {
      "extensions": [".blend", ".fbx"],
      "destination": "C:/Projects/3D"
    }
  ]
}
```

* `user` (optional): simple placeholder replace used anywhere you write `{{user}}`.
* `watch_folder`: folder to monitor. All placeholders are expanded before use.
* `use_default_rules`: toggles the bundled defaults (installer/archive/image/video/audio/doc/web/text groups).
* `default_rules`: optional overrides for the defaults. If omitted, a built-in list points at system folders such as `Pictures`, `Videos`, `Music`, etc.
* `custom_rules`: append your own rules; if both defaults and custom rule match the same extension, the first defined wins.

### 🧪 Verifying the setup
1. Launch the executable from the folder that also contains the `config/` directory.  
2. Confirm the console prints the resolved watch folder and “Startup entry registered successfully.”  
3. Drop a file that matches one of your rules into the watch folder; the console should log the move and the file should appear in the configured destination.
4. Leave the console open to keep watching, or close it after the initial pass if you prefer manual runs.

---

### ⚖️ License

This project is licensed under the MPL 2.0 License - see the `LICENSE` file for details.

