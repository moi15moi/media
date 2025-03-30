#ifndef ANDROIDX_MEDIA3_AUTO_FONTCONFIG_HANDLER_H
#define ANDROIDX_MEDIA3_AUTO_FONTCONFIG_HANDLER_H

#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <android/log.h>
#include <cstdio>
#include <cerrno>

#define FC_TAG "AutoFontconfig"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, FC_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, FC_TAG, __VA_ARGS__)

/**
 * A simple handler that automatically sets up fontconfig without user input
 */
class AutoFontconfigHandler {
 public:
  /**
   * Set up fontconfig automatically
   *
   * This function will:
   * 1. Check for existing fontconfig files in standard locations
   * 2. Create a default fontconfig if none is found
   * 3. Set up appropriate environment variables
   *
   * @return Path to the fontconfig file, or empty string on failure
   */
  static std::string setupFontconfig() {
    // First try to use system fontconfig
    const char* systemPaths[] = {
        "/system/etc/fonts.conf",
        "/vendor/etc/fonts.conf",
        "/product/etc/fonts.conf"
    };

    for (const auto& path : systemPaths) {
      if (fileExists(path)) {
        LOGD("Using system fontconfig: %s", path);
        setenv("FONTCONFIG_FILE", path, 1);
        return path;
      }
    }

    // Try to find existing fontconfig in common locations
    std::vector<std::string> searchPaths = getSearchPaths();
    for (const auto& basePath : searchPaths) {
      std::string confPath = basePath + "/fonts.conf";
      if (fileExists(confPath)) {
        LOGD("Found existing fontconfig at: %s", confPath.c_str());
        setenv("FONTCONFIG_PATH", basePath.c_str(), 1);
        setenv("FONTCONFIG_FILE", confPath.c_str(), 1);
        return confPath;
      }
    }

    // Create a default fontconfig in one of our writable locations
    for (const auto& basePath : searchPaths) {
      if (isWritable(basePath) || createDirIfMissing(basePath)) {
        std::string confPath = basePath + "/fonts.conf";
        if (writeFontconfigFile(confPath, basePath)) {
          LOGD("Created fontconfig at: %s", confPath.c_str());
          setenv("FONTCONFIG_PATH", basePath.c_str(), 1);
          setenv("FONTCONFIG_FILE", confPath.c_str(), 1);
          return confPath;
        }
      }
    }

    // Last resort: try to write to /data/local/tmp which often works
    std::string tmpPath = "/data/local/tmp";
    if (isWritable(tmpPath)) {
      std::string fontconfigDir = tmpPath + "/fontconfig";
      if (createDirIfMissing(fontconfigDir)) {
        std::string confPath = fontconfigDir + "/fonts.conf";
        if (writeFontconfigFile(confPath, fontconfigDir)) {
          LOGD("Created fontconfig in tmp dir: %s", confPath.c_str());
          setenv("FONTCONFIG_PATH", fontconfigDir.c_str(), 1);
          setenv("FONTCONFIG_FILE", confPath.c_str(), 1);
          return confPath;
        }
      }
    }

    LOGE("Failed to set up fontconfig automatically");
    return "";
  }

 private:
  /**
   * Get a list of paths to search for fontconfig
   */
  static std::vector<std::string> getSearchPaths() {
    std::vector<std::string> paths;

    // Get process info to determine our app's data directory
    std::string appDir = getAppDataDir();
    if (!appDir.empty()) {
      paths.push_back(appDir + "/cache/fontconfig");
      paths.push_back(appDir + "/files/fontconfig");
    }

    // Try to get app package name from cmdline
    std::string packageName = getPackageName();
    if (!packageName.empty()) {
      // Common paths for all possible user IDs
      for (const char* userId : {"0", "10", "999"}) {
        paths.push_back("/data/user/" + std::string(userId) + "/" + packageName + "/cache/fontconfig");
        paths.push_back("/data/user/" + std::string(userId) + "/" + packageName + "/files/fontconfig");
      }

      // Direct data path
      paths.push_back("/data/data/" + packageName + "/cache/fontconfig");
      paths.push_back("/data/data/" + packageName + "/files/fontconfig");

      // External storage path - might work on some devices
      paths.push_back("/sdcard/Android/data/" + packageName + "/cache/fontconfig");
      paths.push_back("/sdcard/Android/data/" + packageName + "/files/fontconfig");
    }

    // Add lib-specific paths
    paths.push_back("/data/local/tmp/androidx-media3-fontconfig");
    paths.push_back("/data/local/tmp/fontconfig");

    return paths;
  }

  /**
   * Get app data directory from process information
   */
  static std::string getAppDataDir() {
    std::string result;

    // Try to read app's data directory from /proc/self/cmdline
    FILE* fp = fopen("/proc/self/cmdline", "r");
    if (fp) {
      char buffer[512] = {0};
      if (fread(buffer, 1, sizeof(buffer) - 1, fp) > 0) {
        std::string packageName = buffer; // First string in cmdline is usually package name

        // Check standard data directory paths
        std::string dataPath = "/data/data/" + packageName;
        if (fileExists(dataPath)) {
          result = dataPath;
        } else {
          // Check user directories
          std::string userPath = "/data/user/0/" + packageName;
          if (fileExists(userPath)) {
            result = userPath;
          }
        }
      }
      fclose(fp);
    }

    return result;
  }

  /**
   * Get package name from process information
   */
  static std::string getPackageName() {
    std::string result;

    // Try to read package name from /proc/self/cmdline
    FILE* fp = fopen("/proc/self/cmdline", "r");
    if (fp) {
      char buffer[256] = {0};
      if (fread(buffer, 1, sizeof(buffer) - 1, fp) > 0) {
        result = buffer;
      }
      fclose(fp);
    }

    return result;
  }

  /**
   * Check if a file exists and is readable
   */
  static bool fileExists(const std::string& path) {
    return access(path.c_str(), R_OK) == 0;
  }

  /**
   * Check if a directory is writable
   */
  static bool isWritable(const std::string& path) {
    // Check if exists and is writable
    if (access(path.c_str(), W_OK) == 0) {
      struct stat st;
      if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
      }
    }
    return false;
  }

  /**
   * Create a directory if it doesn't exist
   */
  static bool createDirIfMissing(const std::string& dirPath) {
    // Check if directory already exists
    if (isWritable(dirPath)) {
      return true;
    }

    // Create parent directories recursively
    size_t pos = dirPath.find_last_of('/');
    if (pos != std::string::npos) {
      std::string parentDir = dirPath.substr(0, pos);
      if (!parentDir.empty() && parentDir != dirPath) {
        if (!createDirIfMissing(parentDir)) {
          return false;
        }
      }
    }

    // Create the directory
    if (mkdir(dirPath.c_str(), 0755) == 0 || errno == EEXIST) {
      return isWritable(dirPath);
    }

    return false;
  }

  /**
   * Write a basic fontconfig configuration file
   */
  static bool writeFontconfigFile(const std::string& confPath, const std::string& cacheDir) {
    FILE* confFile = fopen(confPath.c_str(), "w");
    if (!confFile) {
      LOGE("Failed to open file for writing: %s (errno: %d)", confPath.c_str(), errno);
      return false;
    }

    // Write the fontconfig header
    fprintf(confFile, "<?xml version=\"1.0\"?>\n");
    fprintf(confFile, "<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n");
    fprintf(confFile, "<fontconfig>\n");

    // Add font directories - Android-specific paths
    fprintf(confFile, "  <dir>/system/fonts</dir>\n");
    fprintf(confFile, "  <dir>/system/font</dir>\n");
    fprintf(confFile, "  <dir>/vendor/fonts</dir>\n");
    fprintf(confFile, "  <dir>/product/fonts</dir>\n");
    fprintf(confFile, "  <dir>/data/fonts</dir>\n");
    fprintf(confFile, "  <dir>%s/fonts</dir>\n", cacheDir.c_str());

    // Set cache directory
    fprintf(confFile, "  <cachedir>%s</cachedir>\n", cacheDir.c_str());

    // Default alias setup for sans-serif
    fprintf(confFile, "  <match target=\"pattern\">\n");
    fprintf(confFile, "    <test qual=\"any\" name=\"family\">\n");
    fprintf(confFile, "      <string>sans-serif</string>\n");
    fprintf(confFile, "    </test>\n");
    fprintf(confFile, "    <edit name=\"family\" mode=\"assign\" binding=\"same\">\n");
    fprintf(confFile, "      <string>Roboto</string>\n");
    fprintf(confFile, "    </edit>\n");
    fprintf(confFile, "  </match>\n");

    // Default alias setup for serif
    fprintf(confFile, "  <match target=\"pattern\">\n");
    fprintf(confFile, "    <test qual=\"any\" name=\"family\">\n");
    fprintf(confFile, "      <string>serif</string>\n");
    fprintf(confFile, "    </test>\n");
    fprintf(confFile, "    <edit name=\"family\" mode=\"assign\" binding=\"same\">\n");
    fprintf(confFile, "      <string>Noto Serif</string>\n");
    fprintf(confFile, "      <string>DroidSerif</string>\n");
    fprintf(confFile, "      <string>Roboto</string>\n");
    fprintf(confFile, "    </edit>\n");
    fprintf(confFile, "  </match>\n");

    // Default alias setup for monospace
    fprintf(confFile, "  <match target=\"pattern\">\n");
    fprintf(confFile, "    <test qual=\"any\" name=\"family\">\n");
    fprintf(confFile, "      <string>monospace</string>\n");
    fprintf(confFile, "    </test>\n");
    fprintf(confFile, "    <edit name=\"family\" mode=\"assign\" binding=\"same\">\n");
    fprintf(confFile, "      <string>Droid Sans Mono</string>\n");
    fprintf(confFile, "      <string>Roboto Mono</string>\n");
    fprintf(confFile, "    </edit>\n");
    fprintf(confFile, "  </match>\n");

    fprintf(confFile, "</fontconfig>\n");
    fclose(confFile);

    // Verify the file was created successfully
    return fileExists(confPath);
  }
};

#endif // ANDROIDX_MEDIA3_AUTO_FONTCONFIG_HANDLER_H