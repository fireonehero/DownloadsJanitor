# 🧹 DownloadsJanitor

**DownloadsJanitor** is an automated, rules-based file organizer for Windows. Built in modern C++, this tool monitors specified folders (like your 'Downloads') and automatically moves files to their correct destinations based on your custom JSON rules, keeping your system clean and tidy.

---

### ✨ Features

* **Rules-Based Organizing:** Define simple or complex rules in an easy-to-edit `rules.json` file.
* **Automatic Sorting:** Sorts files based on their extensions (e.g., `.png`, `.jpg` -> `Pictures/`).
* **Automatic Folder Creation:** If a destination folder doesn't exist, DownloadsJanitor will create it for you.
* **Modern C++:** Built using C++17 for high-performance I/O.
* **(In-Progress) Background Monitoring:** A future goal is to have the tool run silently in the system tray and watch for new files in real-time.

---

### 🚀 Getting Started

To get a local copy up and running, follow these simple steps.

#### Prerequisites

* A C++17 compatible compiler (e.g., MSVC, GCC, Clang)
* [CMake](httpss://cmake.org/download/) (version 3.10 or higher)
* [Git](httpss://git-scm.com/downloads)

#### Building the Project

1.  **Clone the repo:**
    ```sh
    git clone [https://github.com/your-username/DownloadsJanitor.git](https://github.com/your-username/DownloadsJanitor.git)
    cd DownloadsJanitor
    ```

2.  **Get Submodules (if you add nlohmann/json as a submodule):**
    ```sh
    git submodule update --init --recursive
    ```
    *(Alternatively, manually download the `json.hpp` header into the `external/` folder).*

3.  **Configure and build with CMake:**
    ```sh
    # Create a build directory
    cmake -S . -B build
    
    # Compile the project
    cmake --build build
    ```

4.  **Run the application:**
    The final executable will be in your `build/` directory.
    ```sh
    ./build/DownloadsJanitor.exe
    ```

---

### ⚙️ How to Use

By default, `DownloadsJanitor.exe` will look for a `rules.json` file in its current directory.

#### Configuration (`config/rules.json`)

The `rules.json` file is the brain of the operation. You define which folder to watch and what rules to apply.

```json
{
  "watch_folder": "C:/Users/YourUser/Downloads",
  "rules": [
    {
      "comment": "Move all installers to a specific folder",
      "extensions": [".exe", ".msi"],
      "destination": "C:/Users/YourUser/Downloads/Installers"
    },
    {
      "comment": "Move all compressed archives",
      "extensions": [".zip", ".rar", ".7z"],
      "destination": "C:/Users/YourUser/Downloads/Archives"
    },
    {
      "comment": "Organize images",
      "extensions": [".jpg", ".jpeg", ".png", ".gif"],
      "destination": "C:/Users/YourUser/Pictures/FromDownloads"
    },
    {
      "comment": "Organize documents",
      "extensions": [".pdf", ".docx", ".txt"],
      "destination": "C:/Users/YourUser/Documents/FromDownloads"
    }
  ]
}
```

* `"watch_folder"`: The absolute path to the directory you want to clean up. **Note:** Use forward slashes `/` instead of backslashes `\` in JSON.
* `"rules"`: An array of rule objects.
    * `"extensions"`: A list of file extensions (including the dot) to match.
    * `"destination"`: The absolute path where matched files will be moved.

---

### ⚖️ License

This project is licensed under the MPL 2.0 License - see the `LICENSE` file for details.
