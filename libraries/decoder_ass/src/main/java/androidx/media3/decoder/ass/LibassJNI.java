package androidx.media3.decoder.ass;

import androidx.media3.common.util.Log;

public class LibassJNI {
  private long assLibraryPtr;

  public LibassJNI() {
    if (!AssLibrary.isAvailable()) {
      throw new RuntimeException("Libass native library is not available");
    }

    assLibraryPtr = initAssLibrary();
    if (assLibraryPtr == 0) {
      throw new RuntimeException("Failed to initialize ASS_Library");
    }
  }

  /**
   * Loads a font from its raw byte data and adds it to the ASS_Library.
   *
   * @param fileName The name of the font file.
   * @param fontData The raw byte data of the font.
   */
  public void loadFont(String fileName, byte[] fontData) {
    addFont(assLibraryPtr, fileName, fontData);
  }

  @Override
  protected void finalize() throws Throwable {
    try {
      if (assLibraryPtr != 0) {
        destroyAssLibrary(assLibraryPtr);
      }
    } finally {
      super.finalize();
    }
  }

  /**
   * Adds a font to the ASS_Library.
   *
   * @param assLibraryPtr The pointer to the native ASS_Library instance.
   * @param fontName The name of the font.
   * @param fontData The raw byte data of the font.
   */
  private native void addFont(long assLibraryPtr, String fontName, byte[] fontData);

  /**
   * Initializes the native ASS_Library and returns its pointer as a long.
   * This pointer must be passed to native methods that require it.
   */
  private native long initAssLibrary();

  /**
   * Destroys the native ASS_Library instance.
   *
   * @param assLibraryPtr The pointer to the native ASS_Library instance.
   */
  private native void destroyAssLibrary(long assLibraryPtr);
}
