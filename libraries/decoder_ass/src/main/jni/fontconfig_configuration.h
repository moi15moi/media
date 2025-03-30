#ifndef ANDROIDX_MEDIA3_FONTCONFIG_CONFIGURATION_H
#define ANDROIDX_MEDIA3_FONTCONFIG_CONFIGURATION_H

/**
 * Helper function to generate and write a fontconfig configuration file.
 *
 * @param confPath Path where the config file should be written
 * @param cacheDir Directory to use for fontconfig cache
 * @return true if configuration was written successfully, false otherwise
 */
static bool writeFontconfigFile(const char* confPath, const char* cacheDir) {
  FILE* confFile = fopen(confPath, "w");
  if (!confFile) {
    return false;
  }

  // Write the fontconfig header
  fprintf(confFile, "<?xml version=\"1.0\"?>\n");
  fprintf(confFile, "<!DOCTYPE fontconfig SYSTEM \"fonts.dtd\">\n");
  fprintf(confFile, "<fontconfig>\n");

  // Add font directories
  fprintf(confFile, "  <dir>/system/fonts</dir>\n");
  fprintf(confFile, "  <dir>/system/font</dir>\n");
  fprintf(confFile, "  <dir>/vendor/fonts</dir>\n");
  fprintf(confFile, "  <dir>/product/fonts</dir>\n");
  fprintf(confFile, "  <dir>/data/fonts</dir>\n");

  // Set cache directory
  fprintf(confFile, "  <cachedir>%s</cachedir>\n", cacheDir);

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

  // Common font substitutions - Arial/Helvetica -> Roboto
  fprintf(confFile, "  <match target=\"pattern\">\n");
  fprintf(confFile, "    <test name=\"family\">\n");
  fprintf(confFile, "      <string>Arial</string>\n");
  fprintf(confFile, "    </test>\n");
  fprintf(confFile, "    <edit name=\"family\" mode=\"assign\" binding=\"same\">\n");
  fprintf(confFile, "      <string>Roboto</string>\n");
  fprintf(confFile, "    </edit>\n");
  fprintf(confFile, "  </match>\n");

  fprintf(confFile, "  <match target=\"pattern\">\n");
  fprintf(confFile, "    <test name=\"family\">\n");
  fprintf(confFile, "      <string>Helvetica</string>\n");
  fprintf(confFile, "    </test>\n");
  fprintf(confFile, "    <edit name=\"family\" mode=\"assign\" binding=\"same\">\n");
  fprintf(confFile, "      <string>Roboto</string>\n");
  fprintf(confFile, "    </edit>\n");
  fprintf(confFile, "  </match>\n");

  // Common font substitutions - Times/Times New Roman -> Noto Serif
  fprintf(confFile, "  <match target=\"pattern\">\n");
  fprintf(confFile, "    <test name=\"family\">\n");
  fprintf(confFile, "      <string>Times</string>\n");
  fprintf(confFile, "    </test>\n");
  fprintf(confFile, "    <edit name=\"family\" mode=\"assign\" binding=\"same\">\n");
  fprintf(confFile, "      <string>Noto Serif</string>\n");
  fprintf(confFile, "      <string>DroidSerif</string>\n");
  fprintf(confFile, "      <string>Roboto</string>\n");
  fprintf(confFile, "    </edit>\n");
  fprintf(confFile, "  </match>\n");

  fprintf(confFile, "  <match target=\"pattern\">\n");
  fprintf(confFile, "    <test name=\"family\">\n");
  fprintf(confFile, "      <string>Times New Roman</string>\n");
  fprintf(confFile, "    </test>\n");
  fprintf(confFile, "    <edit name=\"family\" mode=\"assign\" binding=\"same\">\n");
  fprintf(confFile, "      <string>Noto Serif</string>\n");
  fprintf(confFile, "      <string>DroidSerif</string>\n");
  fprintf(confFile, "      <string>Roboto</string>\n");
  fprintf(confFile, "    </edit>\n");
  fprintf(confFile, "  </match>\n");

  fprintf(confFile, "</fontconfig>\n");
  fclose(confFile);

  return true;
}

#endif //ANDROIDX_MEDIA3_FONTCONFIG_CONFIGURATION_H
