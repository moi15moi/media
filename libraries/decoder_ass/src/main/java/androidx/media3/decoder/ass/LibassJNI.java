package androidx.media3.decoder.ass;

import androidx.media3.common.util.Log;
import java.util.HashMap;
import java.util.Map;

public class LibassJNI {
  private final String TAG = "LibassJNI";
  private final long assLibraryPtr;
  private final long assRendererPtr;
  private final Map<String, Long> assTrackPtrs = new HashMap<>();

  public LibassJNI() {
    if (!AssLibrary.isAvailable()) {
      throw new RuntimeException("Libass native library is not available");
    }

    assLibraryPtr = initAssLibrary();
    if (assLibraryPtr == 0) {
      throw new RuntimeException("Failed to initialize ASS_Library");
    }

    assRendererPtr = initAssRenderer(assLibraryPtr);
    if (assRendererPtr == 0) {
      throw new RuntimeException("Failed to initialize ASS_Renderer");
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
   * Creates a new ASS_Track instance if it does not already exist for the given format ID.
   *
   * @param formatId The unique identifier for the format.
   * @throws RuntimeException if the ASS_Track creation fails.
   */
  public void createTrack(String formatId) {
    if (assTrackPtrs.containsKey(formatId)) {
      Long trackPtr = assTrackPtrs.get(formatId);
      if (trackPtr == null || trackPtr == 0) {
        // Invalid state, remove and recreate
        assTrackPtrs.remove(formatId);
        Log.w(TAG, "Invalid track pointer for format ID: '" + formatId);
      } else {
        // Track already exists and is valid, just return
        Log.d(TAG, "Reusing existing track for format ID: " + formatId);
        return;
      }
    }

    long trackPtr = createTrackNative(assLibraryPtr);
    if (trackPtr == 0) {
      throw new RuntimeException("Failed to create ASS_Track");
    }
    assTrackPtrs.put(formatId, trackPtr);
    Log.d(TAG, "Created new track with ID: " + formatId);
  }

  /**
   * Releases a track when it's no longer needed.
   *
   * @param trackId The ID of the track to release.
   */
  public void releaseTrack(String trackId) {
    Long trackPtr = assTrackPtrs.remove(trackId);
    if (trackPtr != null && trackPtr != 0) {
      destroyTrackNative(trackPtr);
      Log.d(TAG, "Released track with ID: " + trackId);
      return;
    }
    Log.w(TAG, "Attempted to release non-existent track ID: " + trackId);
  }

  /**
   * Processes codec private data (subtitle headers) for a specific track.
   *
   * @param trackId The ID of the track to process the data for.
   * @param data    The codec private data bytes.
   */
  public void processCodecPrivate(String trackId, byte[] data) {
    Long trackPtr = assTrackPtrs.get(trackId);
    if (trackPtr != null && trackPtr != 0) {
      processCodecPrivateNative(trackPtr, data);
      Log.d(TAG, "Processed codec private data for track ID: " + trackId);
    } else {
      Log.e(TAG, "Cannot process codec private data: track not found: " + trackId);
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
      for (Map.Entry<String, Long> entry : assTrackPtrs.entrySet()) {
        if (entry.getValue() != 0) {
          destroyTrackNative(entry.getValue());
          Log.d(TAG, "Finalized track: " + entry.getKey());
        }
      }
      assTrackPtrs.clear();

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

  /**
   * Creates a new ASS_Track instance.
   *
   * @param assLibraryPtr The pointer to the native ASS_Library instance.
   * @return The pointer to the created ASS_Track instance.
   */
  private native long createTrackNative(long assLibraryPtr);

  /**
   * Destroys the ASS_Track instance.
   *
   * @param assTrackPtr The pointer to the native ASS_Track instance.
   */
  private native void destroyTrackNative(long assTrackPtr);

  /**
   * Processes codec private data (subtitle headers) for the ASS_Track.
   *
   * @param assTrackPtr The pointer to the native ASS_Track instance.
   * @param data The codec private data bytes.
   */
  private native void processCodecPrivateNative(long assTrackPtr, byte[] data);
}
