package androidx.media3.decoder.ass;

public class LibassJNI {
  private final long assLibraryPtr;
  private final long assRendererPtr;
  private final long assTrackPtr;

  public LibassJNI() {
    if (!AssLibrary.isAvailable()) {
      throw new RuntimeException("Libass native library is not available");
    }

    assLibraryPtr = initAssLibrary();
    if (assLibraryPtr == 0) {
      throw new RuntimeException("Failed to initialize ASS_Library");
    }

    assTrackPtr = createTrackNative(assLibraryPtr);
    if (assTrackPtr == 0) {
      destroyAssLibrary(assLibraryPtr);
      throw new RuntimeException("Failed to create ASS_Track");
    }

    assRendererPtr = initAssRenderer(assLibraryPtr);
    if (assRendererPtr == 0) {
      throw new RuntimeException("Failed to initialize ASS_Renderer");
    }
  }

  /**
   * Processes codec private data (subtitle headers) from a subtitle stream.
   * This is typically called with the codec initialization data from the container.
   *
   * @param data The codec private data bytes.
   */
  public void processCodecPrivate(byte[] data) {
    if (assTrackPtr != 0) {
      processCodecPrivateNative(assTrackPtr, data);
    } else {
      Log.e(TAG, "Cannot process codec private data: track not initialized");
    }
  }

  /**
   * Sets the frame size for the ASS_Renderer.
   *
   * @param width The width of the frame in pixels.
   * @param height The height of the frame in pixels.
   */
  public void setFrameSize(int width, int height) {
      setFrameSizeNative(assRendererPtr, width, height);
  }

  /**
   * Sets the storage size for the ASS_Renderer.
   *
   * @param width The width of the storage in pixels.
   * @param height The height of the storage in pixels.
   */
  public void setStorageSize(int width, int height) {
      setStorageSizeNative(assRendererPtr, width, height);
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
      if (assTrackPtr != 0) {
        destroyTrackNative(assTrackPtr);
      }
      if (assRendererPtr != 0) {
        destroyAssRenderer(assRendererPtr);
      }
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

  /**
   * Initializes the native ASS_Renderer and returns its pointer as a long.
   * This pointer must be passed to native methods that require it.
   *
   * @param assLibraryPtr The pointer to the native ASS_Library instance.
   * @return The pointer to the native ASS_Renderer instance.
   */
  private native long initAssRenderer(long assLibraryPtr);

  /**
   * Destroys the native ASS_Renderer instance.
   *
   * @param assRendererPtr The pointer to the native ASS_Renderer instance.
   */
  private native void destroyAssRenderer(long assRendererPtr);

  /**
   * Sets the frame size for the ASS_Renderer.
   *
   * @param assRendererPtr The pointer to the native ASS_Renderer instance.
   * @param width The width of the frame in pixels.
   * @param height The height of the frame in pixels.
   */
  private native void setFrameSizeNative(long assRendererPtr, int width, int height);

  /**
   * Sets the storage size for the ASS_Renderer.
   *
   * @param assRendererPtr The pointer to the native ASS_Renderer instance.
   * @param width The width of the storage in pixels.
   * @param height The height of the storage in pixels.
   */
  private native void setStorageSizeNative(long assRendererPtr, int width, int height);

  private native long createTrackNative(long assLibraryPtr);
  private native void destroyTrackNative(long assTrackPtr);
  private native void processCodecPrivateNative(long assTrackPtr, byte[] data);
}
