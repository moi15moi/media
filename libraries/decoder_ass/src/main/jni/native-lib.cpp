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

static int g_frame_width = 1280;
static int g_frame_height = 720;

// Fonction de rendu RGBA adaptée de mpv-player
// Mélange les pixels des sous-titres (src) avec le buffer de destination (dst)
// en prenant en compte le canal alpha et la couleur spécifique des sous-titres
// Provient de mpv: https://github.com/mpv-player/mpv/blob/bc96b23ef686d29efe95d54a4fd1836c177d7a61/sub/draw_bmp.c#L295-L338
static void draw_ass_rgba(uint8_t *dst, ptrdiff_t dst_stride,
                          const uint8_t *src, ptrdiff_t src_stride,
                          int w, int h, uint32_t color) {
    // 1. Extraction CORRECTE du format RGBA de libass
    const unsigned int ass_r = (color >> 24) & 0xff; // Rouge (bits 24-31)
    const unsigned int ass_g = (color >> 16) & 0xff; // Vert (bits 16-23)
    const unsigned int ass_b = (color >> 8) & 0xff; // Bleu (bits 8-15)
    const unsigned int ass_a = 0xff - (color & 0xff); // Alpha inversé (ASS utilise 0 = opaque)

    // Parcours de tous les pixels de l'image
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const unsigned int v = src[x]; // Alpha du sous-titre

            // 2. Extraction des composantes DESTINATION (ANDROID_BITMAP_FORMAT_RGBA_8888.)
            unsigned int dst_r = dst[x * 4 + 0];
            unsigned int dst_g = dst[x * 4 + 1];
            unsigned int dst_b = dst[x * 4 + 2];
            unsigned int dst_a = dst[x * 4 + 3];

            // 3. Calcul du blending alpha
            unsigned int aa = ass_a * v;
            unsigned int blend_factor = 255 * 255 - aa;

            // Mélange des canaux (ASS + Destination)
            unsigned int out_r = (v * ass_r * ass_a + dst_r * blend_factor) / (255 * 255);
            unsigned int out_g = (v * ass_g * ass_a + dst_g * blend_factor) / (255 * 255);
            unsigned int out_b = (v * ass_b * ass_a + dst_b * blend_factor) / (255 * 255);
            unsigned int out_a = (aa * 255 + dst_a * blend_factor) / (255 * 255);

            // 4. Réassemblage en RBGA pour Android
            dst[x * 4 + 0] = out_r;
            dst[x * 4 + 1] = out_g;
            dst[x * 4 + 2] = out_b;
            dst[x * 4 + 3] = out_a;
        }
        dst += dst_stride;
        src += src_stride;
    }
}

// Callback de libass pour les messages d'erreur
void libass_msg_callback(int level, const char *fmt, va_list args, void *data) {
  if (level < 6) {
    __android_log_vprint(ANDROID_LOG_DEBUG, "LIBASS_LOG", fmt, args);
  }
}

// Lit un fichier depuis les assets Android
std::unique_ptr<char[]>
read_asset_file(AAssetManager *assetManager, const char *filename, int &length) {
    // Ouverture de l'asset avec gestion automatique de fermeture
    std::unique_ptr<AAsset, decltype(&AAsset_close)> asset(
            AAssetManager_open(assetManager, filename, AASSET_MODE_BUFFER),
            AAsset_close
    );

    if (!asset) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "Failed to open asset: %s", filename);
        return nullptr;
    }

    // Lecture de contenu dans un buffer
    length = AAsset_getLength(asset.get());
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(length + 1);
    AAsset_read(asset.get(), buffer.get(), length);

    return buffer;
}

// Méthode JNI de démonstration
extern "C" JNIEXPORT jstring JNICALL
Java_com_example_prototypelibass_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";

    // Initialisation minimale de libass pour vérifier la version
    ASS_Library *lib = ass_library_init();
    int version = ass_library_version();
    __android_log_print(ANDROID_LOG_DEBUG, "TRACKERS", "Print la version de libass 0x%x", version);
    ass_library_done(lib);

    return env->NewStringUTF(hello.c_str());
}

// Méthode principale de rendu des sous-titres
extern "C"
JNIEXPORT jobject JNICALL
Java_com_example_prototypelibass_MainActivity_renderSubtitleFrame(
        JNIEnv *env,
        jobject thiz,
        jobject asset_manager,
        jint screenWidth,
        jint screenHeight,
        jint timestamp
) {
    // Récupération de l'AssetManager natif
    AAssetManager *g_assetManager = AAssetManager_fromJava(env, asset_manager);

    // Lire le fichier de sous-titres et de police avec la fonction générique
    int subtitleLength, fontLength;
    auto subtitleBuffer = read_asset_file(g_assetManager, "subtitle.ass", subtitleLength);
    auto fontBuffer = read_asset_file(g_assetManager, "C059-Roman.otf", fontLength);
    if (!subtitleBuffer || !fontBuffer) return nullptr;

    // Initialisation de libass avec gestion automatique de mémoire
    std::unique_ptr<ASS_Library, decltype(&ass_library_done)> lib(
            ass_library_init(),
            ass_library_done
    );
    if (!lib) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "ass_library_init failed");
        return nullptr;
    }

    ass_set_message_cb(lib.get(), libass_msg_callback, nullptr);

    // Création du renderer
    std::unique_ptr<ASS_Renderer, decltype(&ass_renderer_done)> renderer(
            ass_renderer_init(lib.get()),
            ass_renderer_done
    );
    if (!renderer) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "ass_renderer_init failed");
        return nullptr;
    }

    // Configuration du renderer
    ass_set_storage_size(renderer.get(), screenWidth, screenHeight);
    ass_set_frame_size(renderer.get(), screenWidth, screenHeight);
    ass_set_fonts(renderer.get(), nullptr, nullptr, ASS_FONTPROVIDER_AUTODETECT, nullptr, true);
    ass_add_font(lib.get(), "C059-Roman.otf", fontBuffer.get(), fontLength);

    // Chargement de la piste de sous-titres
    std::unique_ptr<ASS_Track, decltype(&ass_free_track)> track(
            ass_read_memory(lib.get(), subtitleBuffer.get(), subtitleLength, nullptr),
            ass_free_track
    );
    if (!track) {
        __android_log_print(ANDROID_LOG_ERROR, "TRACKERS", "ass_read_memory failed");
        return nullptr;
    }

    // Rendu de l'image à un timestamp donné
    ASS_Image *ass_image = ass_render_frame(renderer.get(), track.get(), timestamp, nullptr);
    if (!ass_image) {
        __android_log_print(ANDROID_LOG_ERROR, "SUBTITLE_RENDER", "No subtitle image rendered");
        return nullptr;
    }

    // Création d'un Bitmap Android via JNI
    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapMethod = env->GetStaticMethodID(
            bitmapClass,
            "createBitmap",
            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"
    );

    // Création d'un Bitmap ARGB_8888
    jobject bitmap = env->CallStaticObjectMethod(
            bitmapClass,
            createBitmapMethod,
            screenWidth,
            screenHeight,
            env->GetStaticObjectField(
                    env->FindClass("android/graphics/Bitmap$Config"),
                    env->GetStaticFieldID(env->FindClass("android/graphics/Bitmap$Config"),
                                          "ARGB_8888", "Landroid/graphics/Bitmap$Config;")
            )
    );

    // Accès direct aux pixels du Bitmap
    AndroidBitmapInfo bitmapInfo;
    void *pixels = nullptr;
    if (AndroidBitmap_getInfo(env, bitmap, &bitmapInfo) < 0 ||
        AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "SUBTITLE_RENDER", "Unable to lock bitmap pixels");
        return nullptr;
    }

    // Nettoyage du buffer
    memset(pixels, 0, bitmapInfo.stride * bitmapInfo.height);

    // Dessin de toutes les composantes du sous-titre
    for (ASS_Image *img = ass_image; img; img = img->next) {
        uint8_t *dst = reinterpret_cast<uint8_t *>(pixels) +
                       img->dst_y * bitmapInfo.stride + // Décalage vertical
                       img->dst_x * 4;                  // Décalage horizontal (4 bytes/pixel)

        draw_ass_rgba(dst, (ptrdiff_t) bitmapInfo.stride, img->bitmap, img->stride, img->w, img->h,
                      img->color);
    }

    AndroidBitmap_unlockPixels(env, bitmap);

    return bitmap;
}

// Function to initialize the ASS_Library
extern "C"
JNIEXPORT jlong JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_initAssLibrary(JNIEnv *env, jclass thiz) {
  // Initialize the ASS_Library
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
extern "C"
JNIEXPORT void JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_destroyAssLibrary(JNIEnv *env, jclass clazz, jlong ass_library_ptr) {
  ASS_Library *library = reinterpret_cast<ASS_Library *>(ass_library_ptr);
  if (library) {
    ass_library_done(library);
    LOGD("ASS_Library destroyed successfully");
  } else {
    LOGE("ASS_Library pointer is null during destruction");
  }
}

// add fonts to the library
extern "C"
JNIEXPORT void JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_addFont(
    JNIEnv *env,
    jobject thiz,
    jlong ass_library_ptr,
    jstring font_name,
    jbyteArray font_data
) {
  // Convert jlong back to ASS_Library pointer
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

extern "C"
JNIEXPORT void JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_processChunkNative(JNIEnv *env,
jobject thiz, jlong track, jbyteArray eventData, jint offset, jint length, jlong timecode, jlong duration) {
  jbyte *data = env->GetByteArrayElements(eventData, nullptr);
  if (!data) {
    LOGE("Failed to get data");
    return;
  }

  ass_process_chunk(reinterpret_cast<ASS_Track *>(track),
                    reinterpret_cast<const char *>(data + offset), length, timecode, duration);
  env->ReleaseByteArrayElements(eventData, data, 0);
}


/**
 * Initializes the ASS_Renderer instance.
 *
 * @param env The JNI environment pointer.
 * @param thiz The Java object calling this function.
 * @param ass_library_ptr The pointer to the ASS_Library instance.
 * @return The pointer to the initialized ASS_Renderer instance, or 0 if initialization fails.
 */
extern "C"
JNIEXPORT jlong JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_initAssRenderer(JNIEnv *env, jobject thiz, jlong ass_library_ptr) {
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

  // Basic configuration of the renderer
  ass_set_fonts(renderer, NULL, NULL, ASS_FONTPROVIDER_AUTODETECT, NULL, 1);

  LOGD("ASS_Renderer initialized successfully");
  return reinterpret_cast<jlong>(renderer);
}

/**
 * Destroys the ASS_Renderer instance.
 *
 * @param env The JNI environment pointer.
 * @param thiz The Java object calling this function.
 * @param ass_renderer_ptr The pointer to the ASS_Renderer instance.
 */
extern "C"
JNIEXPORT void JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_destroyAssRenderer(JNIEnv *env, jobject thiz, jlong ass_renderer_ptr) {
  ASS_Renderer *renderer = reinterpret_cast<ASS_Renderer *>(ass_renderer_ptr);
  if (renderer) {
    ass_renderer_done(renderer);
    LOGD("ASS_Renderer destroyed successfully");
  } else {
    LOGE("ASS_Renderer pointer is null during destruction");
  }
}

/**
 * Sets the frame size for the ASS_Renderer.
 *
 * @param env The JNI environment pointer.
 * @param thiz The Java object calling this function.
 * @param ass_renderer_ptr The pointer to the ASS_Renderer instance.
 * @param width The width of the frame in pixels.
 * @param height The height of the frame in pixels.
 */
extern "C"
JNIEXPORT void JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_setFrameSizeNative(JNIEnv *env, jobject thiz, jlong ass_renderer_ptr, jint width, jint height) {
  LOGD("setFrameSizeNative called with renderer=%p, width=%d, height=%d", (void*)ass_renderer_ptr, width, height);

  ASS_Renderer *renderer = reinterpret_cast<ASS_Renderer *>(ass_renderer_ptr);
  if (!renderer) {
    LOGE("ASS_Renderer pointer is null when setting frame size");
    return;
  }

  //TODO: This is probably what causes the subtitles to appear at the wrong resolution right after loading the video
  g_frame_height = height;
  g_frame_width = width;

  LOGD("Calling ass_set_frame_size...");
  ass_set_frame_size(renderer, width, height);
  LOGD("ass_set_frame_size completed successfully");
}

/**
 * Sets the storage size for the ASS_Renderer.
 *
 * @param env The JNI environment pointer.
 * @param thiz The Java object calling this function.
 * @param ass_renderer_ptr The pointer to the ASS_Renderer instance.
 * @param width The width of the storage in pixels.
 * @param height The height of the storage in pixels.
 */
extern "C"
JNIEXPORT void JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_setStorageSizeNative(JNIEnv *env, jobject thiz, jlong ass_renderer_ptr, jint width, jint height) {
  ASS_Renderer *renderer = reinterpret_cast<ASS_Renderer *>(ass_renderer_ptr);
  if (!renderer) {
    LOGE("ASS_Renderer pointer is null when setting storage size");
    return;
  }

  ass_set_storage_size(renderer, width, height);
  LOGD("Storage size set to %d x %d", width, height);
}

/**
 * Creates a new ASS_Track.
 *
 * @param env The JNI environment pointer.
 * @param thiz The Java object calling this function.
 * @param ass_library_ptr The pointer to the ASS_Library instance.
 * @return The pointer to the created ASS_Track instance, or 0 if creation fails.
 */
extern "C"
JNIEXPORT jlong JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_createTrackNative(JNIEnv *env, jobject thiz, jlong ass_library_ptr) {
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

/**
 * Destroys the ASS_Track instance.
 *
 * @param env The JNI environment pointer.
 * @param thiz The Java object calling this function.
 * @param ass_track_ptr The pointer to the ASS_Track instance.
 */
extern "C"
JNIEXPORT void JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_destroyTrackNative(JNIEnv *env, jobject thiz, jlong ass_track_ptr) {
  ASS_Track *track = reinterpret_cast<ASS_Track *>(ass_track_ptr);
  if (track) {
    ass_free_track(track);
    LOGD("ASS_Track destroyed successfully");
  } else {
    LOGE("ASS_Track pointer is null during destruction");
  }
}

/**
 * Processes codec private data from a subtitle stream.
 *
 * @param env The JNI environment pointer.
 * @param thiz The Java object calling this function.
 * @param ass_track_ptr The pointer to the ASS_Track instance.
 * @param data The codec private data.
 */
extern "C"
JNIEXPORT void JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_processCodecPrivateNative(JNIEnv *env, jobject thiz, jlong ass_track_ptr, jbyteArray data) {
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
extern "C"
JNIEXPORT jobject JNICALL
Java_androidx_media3_decoder_ass_LibassJNI_renderFrameNative(
    JNIEnv *env,
    jobject thiz,
    jlong ass_renderer_ptr,
    jlong ass_track_ptr,
    jlong time_ms)
{
  ASS_Renderer *renderer = reinterpret_cast<ASS_Renderer *>(ass_renderer_ptr);
  ASS_Track *track = reinterpret_cast<ASS_Track *>(ass_track_ptr);

  if (!renderer || !track) {
    LOGE("Invalid pointers: renderer=%p, track=%p", renderer, track);
    return nullptr;
  }


  // Check for changes in subtitle display
  int detect_change = 1;

  // Render frame with libass
  ASS_Image *img = ass_render_frame(renderer, track, time_ms, &detect_change);

  // If no images to render, return null
  if (!img) {
    return nullptr;
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
      g_frame_width,  // Use stored width
      g_frame_height, // Use stored height
      env->GetStaticObjectField(
          env->FindClass("android/graphics/Bitmap$Config"),
          env->GetStaticFieldID(env->FindClass("android/graphics/Bitmap$Config"),
                                "ARGB_8888", "Landroid/graphics/Bitmap$Config;")
      )
  );

  // Lock pixels for direct manipulation
  AndroidBitmapInfo bitmapInfo;
  void *pixels = nullptr;
  if (AndroidBitmap_getInfo(env, bitmap, &bitmapInfo) != ANDROID_BITMAP_RESULT_SUCCESS ||
      AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
    LOGE("Unable to lock bitmap pixels");
    return nullptr;
  }



  // Render all subtitle image components
  for (ASS_Image *current = img; current; current = current->next) {
    // Skip images with zero dimensions
    if (current->w <= 0 || current->h <= 0) {
      continue;
    }


    // Calculate destination pointer in bitmap
    uint8_t *dst = reinterpret_cast<uint8_t *>(pixels) +
        current->dst_y * bitmapInfo.stride +  // Vertical offset
        current->dst_x * 4;                   // Horizontal offset (4 bytes per pixel)

    // Render the ASS image component onto the bitmap
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

  // Unlock the bitmap
  AndroidBitmap_unlockPixels(env, bitmap);

  return bitmap;
}