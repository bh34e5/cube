#include "graphics.h"

static char *p_severity[] = {"High", "Medium", "Low", "Notification"};
static char *p_type[] = {"Error",       "Deprecated",  "Undefined",
                         "Portability", "Performance", "Other"};
static char *p_source[] = {"OpenGL",    "OS",          "GLSL Compiler",
                           "3rd Party", "Application", "Other"};

static void debug_callback(uint32_t source, uint32_t type, uint32_t id,
                           uint32_t severity, int32_t length,
                           char const *p_message, void *p_user_param) {
    uint32_t sev_id = 3;
    uint32_t type_id = 5;
    uint32_t source_id = 5;

    // Get the severity
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        sev_id = 0;
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        sev_id = 1;
        break;
    case GL_DEBUG_SEVERITY_LOW:
        sev_id = 2;
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    default:
        sev_id = 3;
        break;
    }

    // Get the type
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        type_id = 0;
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        type_id = 1;
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        type_id = 2;
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        type_id = 3;
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        type_id = 4;
        break;
    case GL_DEBUG_TYPE_OTHER:
    default:
        type_id = 5;
        break;
    }

    // Get the source
    switch (source) {
    case GL_DEBUG_SOURCE_API:
        source_id = 0;
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        source_id = 1;
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        source_id = 2;
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        source_id = 3;
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        source_id = 4;
        break;
    case GL_DEBUG_SOURCE_OTHER:
    default:
        source_id = 5;
        break;
    }

    // Output to the Log
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION,
                    "OpenGL Debug: Severity=%s, Type=%s, Source=%s - %s",
                    p_severity[sev_id], p_type[type_id], p_source[source_id],
                    p_message);

    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        // This a serious error so we need to shutdown the program
        SDL_Event event;
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
    }
}

static void gl_debug_init(void) {
    uint32_t unused_ids = 0;

    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    glDebugMessageCallback((GLDEBUGPROC)&debug_callback, NULL);

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0,
                          &unused_ids, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                          GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
}
