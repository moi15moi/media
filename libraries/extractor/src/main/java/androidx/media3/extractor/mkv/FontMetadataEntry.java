package androidx.media3.extractor.mkv;

import android.os.Parcel;
import android.os.Parcelable;
import androidx.media3.common.Metadata;

/**
 * Represents a font attachment in the MKV file.
 */
public class FontMetadataEntry implements Metadata.Entry {
  private final String fileName;
  private final byte[] fontData;

  public FontMetadataEntry(String fileName, byte[] fontData) {
    this.fileName = fileName;
    this.fontData = fontData;
  }

  public String getFileName() {
    return fileName;
  }

  public byte[] getFontData() {
    return fontData;
  }

}
