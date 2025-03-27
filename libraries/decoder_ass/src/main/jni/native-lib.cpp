#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>
#include <string>
#include <android/bitmap.h>
#include "ass/ass.h"
#include "ass/ass_types.h"

#define LOG_TAG "assNative-lib"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)


static void draw_ass_rgba(uint8_t *dst, ptrdiff_t dst_stride,
                          const uint8_t *src, ptrdiff_t src_stride,
                          int w, int h, uint32_t color) {
  const uint8_t ass_r = (color >> 24) & 0xff; // Red (bits 24-31)
  const uint8_t ass_g = (color >> 16) & 0xff; // Green (bits 16-23)
  const uint8_t ass_b = (color >> 8) & 0xff; // Blue (bits 8-15)
  const uint8_t ass_a = 0xff - (color & 0xff); // Inverted Alpha (ASS uses 0 = opaque)

  // From libass: https://github.com/libass/libass/blob/1b699559025185e34d21a24cac477ca360cb917d/test/test.c#L149-L165
  const uint16_t ROUNDING_OFFSET = 255 * 255 / 2;
  for (size_t y = 0; y < h; y++) {
    for (size_t x = 0; x < w; x++) {
      uint16_t k = src[x] * ass_a;
      dst[x * 4 + 0] = (k * ass_r + (255 * 255 - k) * dst[x * 4 + 0] + ROUNDING_OFFSET) / (255 * 255);
      dst[x * 4 + 1] = (k * ass_g + (255 * 255 - k) * dst[x * 4 + 1] + ROUNDING_OFFSET) / (255 * 255);
      dst[x * 4 + 2] = (k * ass_b + (255 * 255 - k) * dst[x * 4 + 2] + ROUNDING_OFFSET) / (255 * 255);
      dst[x * 4 + 3] = (k * 255   + (255 * 255 - k) * dst[x * 4 + 3] + ROUNDING_OFFSET) / (255 * 255);
    }
    src += src_stride;
    dst += dst_stride;
  }
}

// Callback de libass pour les messages d'erreur
void libass_msg_callback(int level, const char *fmt, va_list args, void *data) {
  if (level < 6) {
    __android_log_vprint(ANDROID_LOG_DEBUG, "LIBASS_LOG", fmt, args);
  }
}

// Function to initialize the ASS_Library
LIBASS_FUNC(jlong, assLibraryInit) {
  ASS_Library *library = ass_library_init();
  if (!library) {
    LOGE("Failed to initialize ASS_Library");
    return reinterpret_cast<jlong>(nullptr);
  }
  LOGD("ASS_Library initialized successfully");

  ass_set_message_cb(library, libass_msg_callback, nullptr);
  return reinterpret_cast<jlong>(library);
}

// Destroy ASS_Library
LIBASS_FUNC(void, assLibraryDone, jlong ass_library_ptr) {
  ASS_Library *library = reinterpret_cast<ASS_Library *>(ass_library_ptr);
  if (library) {
    ass_library_done(library);
    LOGD("ASS_Library destroyed successfully");
  } else {
    LOGE("ASS_Library pointer is null during destruction");
  }
}

// add fonts to the library
LIBASS_FUNC(void, assAddFont, jlong ass_library_ptr, jstring font_name, jbyteArray font_data) {
  ASS_Library *library = reinterpret_cast<ASS_Library *>(ass_library_ptr);
  if (!library) {
    LOGE("ASS_Library pointer is null");
    return;
  }

  // Convert jstring to const char *
  const char *name = env->GetStringUTFChars(font_name, nullptr);
  if (!name) {
    LOGE("Failed to convert font name to UTF-8");
    return;
  }
  LOGD("Adding font: %s", name);

  // Convert jbyteArray to const char *
  jbyte *data = env->GetByteArrayElements(font_data, nullptr);
  if (!data) {
    LOGE("Failed to get font data from byte array");
    env->ReleaseStringUTFChars(font_name, name);
    return;
  }
  jsize data_size = env->GetArrayLength(font_data);
  LOGD("Font data size: %d bytes", data_size);

  ass_add_font(library, name, reinterpret_cast<const char *>(data), data_size);
  LOGD("Font added successfully: %s", name);

  // Release the JNI resources
  env->ReleaseStringUTFChars(font_name, name);
  env->ReleaseByteArrayElements(font_data, data, 0);
}

// Prepare data for processChunk
LIBASS_FUNC(void, assProcessChunk, jlong track, jbyteArray eventData,
            jint offset, jint length, jlong timecode, jlong duration) {
  jbyte *data = env->GetByteArrayElements(eventData, nullptr);
  if (!data) {
    LOGE("Failed to get data");
    return;
  }

  ass_process_chunk(reinterpret_cast<ASS_Track *>(track),
                    reinterpret_cast<const char *>(data + offset), length, timecode, duration);
  env->ReleaseByteArrayElements(eventData, data, 0);
}


// Initialize the ASS_Renderer
LIBASS_FUNC(jlong, assRendererInit, jlong ass_library_ptr) {
  ASS_Library *library = reinterpret_cast<ASS_Library *>(ass_library_ptr);
  if (!library) {
    LOGE("ASS_Library pointer is null during renderer initialization");
    return NULL;
  }

  ASS_Renderer *renderer = ass_renderer_init(library);
  if (!renderer) {
    LOGE("Failed to initialize ASS_Renderer");
    return NULL;
  }

  ass_set_fonts(renderer, NULL, NULL, ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
  LOGD("ASS_Renderer initialized successfully");
  return reinterpret_cast<jlong>(renderer);
}


// Destroy Ass Renderer instance
LIBASS_FUNC(void, assRendererDone, jlong ass_renderer_ptr) {
  ASS_Renderer *renderer = reinterpret_cast<ASS_Renderer *>(ass_renderer_ptr);
  if (renderer) {
    ass_renderer_done(renderer);
    LOGD("ASS_Renderer destroyed successfully");
  } else {
    LOGE("ASS_Renderer pointer is null during destruction");
  }
}

// Sets the frame size for the ASS_Renderer.
LIBASS_FUNC(void, assSetFrameSize, jlong ass_renderer_ptr, jint width, jint height) {
  LOGD("setFrameSizeNative called with renderer=%p, width=%d, height=%d",
       (void *) ass_renderer_ptr, width, height);

  ASS_Renderer *renderer = reinterpret_cast<ASS_Renderer *>(ass_renderer_ptr);
  if (!renderer) {
    LOGE("ASS_Renderer pointer is null when setting frame size");
    return;
  }

  LOGD("Calling ass_set_frame_size...");
  ass_set_frame_size(renderer, width, height);
  LOGD("ass_set_frame_size completed successfully");
}


// Sets the storage size for the ASS_Renderer.
LIBASS_FUNC(void, assSetStorageSize, jlong ass_renderer_ptr, jint width, jint height) {
  ASS_Renderer *renderer = reinterpret_cast<ASS_Renderer *>(ass_renderer_ptr);
  if (!renderer) {
    LOGE("ASS_Renderer pointer is null when setting storage size");
    return;
  }

  ass_set_storage_size(renderer, width, height);
  LOGD("Storage size set to %d x %d", width, height);
}

// Creates new ASS_Track
LIBASS_FUNC(jlong, assNewTrack, jlong ass_library_ptr) {
  auto *library = reinterpret_cast<ASS_Library *>(ass_library_ptr);
  if (!library) {
    LOGE("ASS_Library pointer is null when creating track");
    return NULL;
  }

  ASS_Track *track = ass_new_track(library);
  if (!track) {
    LOGE("Failed to create ASS_Track");
    return NULL;
  }

  LOGD("ASS_Track created successfully");
  return reinterpret_cast<jlong>(track);
}


// Destroys the ASS_Track instance.
LIBASS_FUNC(void, assFreeTrack, jlong ass_track_ptr) {
  ASS_Track *track = reinterpret_cast<ASS_Track *>(ass_track_ptr);
  if (track) {
    ass_free_track(track);
    LOGD("ASS_Track destroyed successfully");
  } else {
    LOGE("ASS_Track pointer is null during destruction");
  }
}

// Process codec private data
LIBASS_FUNC(void, assProcessCodecPrivate, jlong ass_track_ptr, jbyteArray data) {
  ASS_Track *track = reinterpret_cast<ASS_Track *>(ass_track_ptr);
  if (!track) {
    LOGE("ASS_Track pointer is null when processing codec private data");
    return;
  }

  // Convert jbyteArray to const char *
  jbyte *data_bytes = env->GetByteArrayElements(data, nullptr);
  if (!data_bytes) {
    LOGE("Failed to get codec private data from byte array");
    return;
  }
  jsize data_size = env->GetArrayLength(data);

  // Call the libass function
  ass_process_codec_private(track, reinterpret_cast<const char *>(data_bytes), data_size);

  // Release the JNI resources
  env->ReleaseByteArrayElements(data, data_bytes, JNI_OK);
  LOGD("Codec private data processed successfully");
}


/**
 * Renders a frame for a specific track at the given timestamp.
 * This implementation includes diagnostic logging to help debug rendering issues.
 */
LIBASS_FUNC(jobject, renderFrameNative, jlong ass_renderer_ptr, jlong ass_track_ptr, jint frame_width, jint frame_height, jlong time_ms) {
  ASS_Renderer *renderer = reinterpret_cast<ASS_Renderer *>(ass_renderer_ptr);
  ASS_Track *track = reinterpret_cast<ASS_Track *>(ass_track_ptr);

  if (!renderer || !track) {
    LOGE("Invalid pointers: renderer=%p, track=%p", renderer, track);
    return env->NewObject(resultClass, resultConstructor, (jobject) nullptr, JNI_FALSE);
  }

  int detect_change;
  ASS_Image *img = ass_render_frame(renderer, track, time_ms, &detect_change);
  jboolean changedSinceLastCall = detect_change ? JNI_TRUE : JNI_FALSE;

  if (!detect_change || !img) {
    return env->NewObject(resultClass, resultConstructor, (jobject) nullptr, changedSinceLastCall);
  }

  // Create an Android Bitmap
  jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
  jmethodID createBitmapMethod = env->GetStaticMethodID(
      bitmapClass,
      "createBitmap",
      "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"
  );

  // Use ARGB_8888 configuration for transparent bitmap
  jobject bitmap = env->CallStaticObjectMethod(
      bitmapClass,
      createBitmapMethod,
      frame_width,
      frame_height,
      env->GetStaticObjectField(
          env->FindClass("android/graphics/Bitmap$Config"),
          env->GetStaticFieldID(env->FindClass("android/graphics/Bitmap$Config"),
                                "ARGB_8888", "Landroid/graphics/Bitmap$Config;")
      )
  );

  AndroidBitmapInfo bitmapInfo;
  void *pixels = nullptr;
  if (AndroidBitmap_getInfo(env, bitmap, &bitmapInfo) != ANDROID_BITMAP_RESULT_SUCCESS ||
      AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
    LOGE("Unable to lock bitmap pixels");
    return env->NewObject(resultClass, resultConstructor, (jobject) nullptr, changedSinceLastCall);
  }

  for (ASS_Image *current = img; current; current = current->next) {
    if (current->w <= 0 || current->h <= 0) {
      continue;
    }

    uint8_t *dst = reinterpret_cast<uint8_t *>(pixels) +
        current->dst_y * bitmapInfo.stride + // Vertical offset
        current->dst_x * 4;                  // Horizontal offset (4 bytes per pixel)

    draw_ass_rgba(
        dst,
        bitmapInfo.stride,
        current->bitmap,
        current->stride,
        current->w,
        current->h,
        current->color
    );
  }

  AndroidBitmap_unlockPixels(env, bitmap);

  return env->NewObject(resultClass, resultConstructor, bitmap, changedSinceLastCall);
}