package androidx.media3.decoder.ass;

import android.graphics.Bitmap;
import androidx.annotation.Nullable;

public class AssRenderResult {
  @Nullable public final Bitmap bitmap;
  public final boolean changedSinceLastCall;

  public AssRenderResult(@Nullable Bitmap bitmap, boolean changedSinceLastCall) {
    this.bitmap = bitmap;
    this.changedSinceLastCall = changedSinceLastCall;
  }
}
