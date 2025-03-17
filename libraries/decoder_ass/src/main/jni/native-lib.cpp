#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>
#include <string>
#include <android/bitmap.h>
#include "ass/ass.h"
#include "ass/ass_types.h"


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
    __android_log_vprint(ANDROID_LOG_DEBUG, "LIBASS_LOG", fmt, args);
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
