package androidx.media3.decoder.ass;

import androidx.media3.common.util.Log;
import androidx.media3.extractor.text.ssa.SsaParser;
import java.nio.ByteBuffer;
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
      return;
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
    destroyTrackNative(trackPtr);
    Log.d(TAG, "Released track with ID: " + trackId);
  }

  /**
   * Prepares and formats data to then call {@link #assProcessChunk}()}.
   *
   * @param data The ass subtitle event.
   * @param timecode The timestamp in milliseconds.
   * @param trackId The ID of the track to process subtitles from.
   */
  public void prepareProcessChunk(ByteBuffer data, long timecode, String trackId) {

    byte[] bytes = data.array();
    int len = bytes.length;

    // Find the first and second comma positions
    int firstComma = -1, secondComma = -1, commaCount = 0;
    for (int i = 0; i < len; i++) {
      if (bytes[i] == ',') {
        commaCount++;
        if (commaCount == 1) {
          firstComma = i;
        }
        else {
          secondComma = i;
          break;
        }
      }
    }

    // If event formatting is wrong
    if (secondComma == -1) {
      // TODO: Handle this case, maybe skip the subtitle altogether?
    }

    // Create a new byte array excluding the timestamp portion
    int newLength = len - secondComma - 1;
    byte[] newBytes = new byte[newLength];
    System.arraycopy(bytes, secondComma + 1, newBytes, 0, newLength);

    // Extract the timestamp
    int timestampLength = secondComma - firstComma - 1;
    byte[] timestampBytes = new byte[timestampLength];
    System.arraycopy(bytes, firstComma + 1, timestampBytes, 0, timestampLength);
    Log.d(TAG, "Extracted Bytes: " + new String(timestampBytes));

    long durationMs = SsaParser.parseTimecodeUs(new String(timestampBytes)) / 1000;
    Log.d(TAG, "durationMs: " + durationMs);

    Long trackPtr = assTrackPtrs.get(trackId);
    if (trackPtr != null && trackPtr != 0) {
      assProcessChunk(trackPtr, newBytes, newBytes.length, timecode, durationMs);
    } else {
      Log.e(TAG, "Cannot process chunk: track not found: " + trackId);
    }
  }

  /**
   * Processes codec private data (subtitle headers) for a specific track.
   *
   * @param trackId The ID of the track to process the data for.
   * @param data    The codec private data bytes.
   */
  public void processCodecPrivate(String trackId, byte[] data) {
    Long trackPtr = assTrackPtrs.get(trackId);
    processCodecPrivateNative(trackPtr, data);
    Log.d(TAG, "Processed codec private data for track ID: " + trackId);
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
   * Process a chunk of subtitle stream format
   *
   * @param
   */

  private native void assProcessChunk(long assTrackPtr, byte[] data, int size, long timecode, long duration);

  /**
   * Processes codec private data (subtitle headers) for the ASS_Track.
   *
   * @param assTrackPtr The pointer to the native ASS_Track instance.
   * @param data The codec private data bytes.
   */
  private native void processCodecPrivateNative(long assTrackPtr, byte[] data);
}
