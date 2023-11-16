//
// Copyright (C) YuqiaoZhang(HanetakaChou)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include <stddef.h>
#include <stdint.h>
#include "camera_controller.h"
#include "renderer.h"

static class renderer *g_renderer = NULL;
static int32_t g_window_width = -1;
static int32_t g_window_height = -1;

#if defined(__GNUC__)

#if defined(__linux__)

#if defined(__ANDROID__)
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <android/native_activity.h>

static void ANativeActivity_Destroy(ANativeActivity *native_activity);
static void ANativeActivity_WindowFocusChanged(ANativeActivity *native_activity, int hasFocus);
static void ANativeActivity_NativeWindowCreated(ANativeActivity *native_activity, ANativeWindow *native_window);
static void ANativeActivity_NativeWindowResized(ANativeActivity *native_activity, ANativeWindow *native_window);
static void ANativeActivity_NativeWindowRedrawNeeded(ANativeActivity *native_activity, ANativeWindow *native_window);
static void ANativeActivity_NativeWindowDestroyed(ANativeActivity *native_activity, ANativeWindow *native_window);
static void ANativeActivity_InputQueueCreated(ANativeActivity *native_activity, AInputQueue *input_queue);
static void ANativeActivity_InputQueueDestroyed(ANativeActivity *native_activity, AInputQueue *input_queue);

static bool g_this_process_has_inited = false;

static int g_main_thread_looper_draw_callback_fd_read = -1;
static int g_main_thread_looper_draw_callback_fd_write = -1;

static int main_thread_looper_draw_callback(int fd, int, void *);

static pthread_t g_draw_request_thread;
bool volatile g_draw_request_thread_running = false;

static void *draw_request_thread_main(void *);

static int main_thread_looper_input_queue_callback(int fd, int, void *input_queue_void);

extern "C" JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *native_activity, void *saved_state, size_t saved_state_size)
{
    native_activity->callbacks->onStart = NULL;
    native_activity->callbacks->onResume = NULL;
    native_activity->callbacks->onSaveInstanceState = NULL;
    native_activity->callbacks->onPause = NULL;
    native_activity->callbacks->onStop = NULL;
    native_activity->callbacks->onDestroy = ANativeActivity_Destroy;
    native_activity->callbacks->onWindowFocusChanged = ANativeActivity_WindowFocusChanged;
    native_activity->callbacks->onNativeWindowCreated = ANativeActivity_NativeWindowCreated;
    native_activity->callbacks->onNativeWindowResized = ANativeActivity_NativeWindowResized;
    native_activity->callbacks->onNativeWindowRedrawNeeded = ANativeActivity_NativeWindowRedrawNeeded;
    native_activity->callbacks->onNativeWindowDestroyed = ANativeActivity_NativeWindowDestroyed;
    native_activity->callbacks->onInputQueueCreated = ANativeActivity_InputQueueCreated;
    native_activity->callbacks->onInputQueueDestroyed = ANativeActivity_InputQueueDestroyed;
    native_activity->callbacks->onContentRectChanged = NULL;
    native_activity->callbacks->onConfigurationChanged = NULL;
    native_activity->callbacks->onLowMemory = NULL;

    if (!g_this_process_has_inited)
    {
        g_renderer = renderer_init(NULL);

        // Simulate the following callback on Android:
        // WM_PAINT
        // CVDisplayLinkSetOutputCallback
        // displayLinkWithTarget
        {
            // the looper of the main thread
            ALooper *main_thread_looper = ALooper_forThread();
            assert(main_thread_looper != NULL);

            // Evidently, the real "draw" is slower than the "request"
            // There are no message boundaries for "SOCK_STREAM", and We can read all the data once
            int sv[2];
            int res_socketpair = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
            assert(0 == res_socketpair);
            g_main_thread_looper_draw_callback_fd_read = sv[0];
            g_main_thread_looper_draw_callback_fd_write = sv[1];

            // the identifier is ignored when callback is not NULL (the ALooper_pollOnce always returns the ALOOPER_POLL_CALLBACK)
            int res_looper_add_fd = ALooper_addFd(main_thread_looper, g_main_thread_looper_draw_callback_fd_read, -1, ALOOPER_EVENT_INPUT, main_thread_looper_draw_callback, NULL);
            assert(1 == res_looper_add_fd);

            // the "draw request" thread
            // TODO: use Load/Store
            g_draw_request_thread_running = false;
            int res_ptread_create = pthread_create(&g_draw_request_thread, NULL, draw_request_thread_main, NULL);
            assert(0 == res_ptread_create);
            while (!g_draw_request_thread_running)
            {
                // pthread_yield();
                sched_yield();
            }
        }

        // Don't worry
        // Single thread
        g_this_process_has_inited = true;
    }
}

// TODO:
// destroy before this process terminates

static void ANativeActivity_Destroy(ANativeActivity *native_activity)
{
}

static void ANativeActivity_WindowFocusChanged(ANativeActivity *native_activity, int hasFocus)
{
}

static void ANativeActivity_NativeWindowCreated(ANativeActivity *native_activity, ANativeWindow *native_window)
{
    g_window_width = ANativeWindow_getWidth(native_window);
    g_window_height = ANativeWindow_getHeight(native_window);

    renderer_attach_window(g_renderer, native_window);
}

static void ANativeActivity_NativeWindowResized(ANativeActivity *native_activity, ANativeWindow *native_window)
{
}

static void ANativeActivity_NativeWindowRedrawNeeded(ANativeActivity *native_activity, ANativeWindow *native_window)
{
}

static void ANativeActivity_NativeWindowDestroyed(ANativeActivity *native_activity, ANativeWindow *native_window)
{
    renderer_dettach_window(g_renderer);

    g_window_width = -1;
    g_window_height = -1;
}

static void ANativeActivity_InputQueueCreated(ANativeActivity *native_activity, AInputQueue *input_queue)
{
    // the looper of the main thread
    ALooper *main_thread_looper = ALooper_forThread();
    assert(NULL != main_thread_looper);

    // the identifier is ignored when callback is not NULL (the ALooper_pollOnce always returns the ALOOPER_POLL_CALLBACK)
    AInputQueue_attachLooper(input_queue, main_thread_looper, 0, main_thread_looper_input_queue_callback, input_queue);
}

static void ANativeActivity_InputQueueDestroyed(ANativeActivity *native_activity, AInputQueue *input_queue)
{
    ALooper *looper = ALooper_forThread();
    assert(NULL != looper);

    AInputQueue_detachLooper(input_queue);
}

static int main_thread_looper_draw_callback(int fd, int, void *)
{
    // Evidently, the real "draw" is slower than the "request"
    // There are no message boundaries for "SOCK_STREAM", and We can read all the data once
    {
        uint8_t buf[4096];
        ssize_t res_recv;
        while ((-1 == (res_recv = recv(fd, buf, 4096U, 0))) && (EINTR == errno))
        {
            // pthread_yield();
            sched_yield();
        }
        assert(-1 != res_recv);
    }

    // draw
    renderer_draw(g_renderer);

    return 1;
}

static void *draw_request_thread_main(void *)
{
    g_draw_request_thread_running = true;

    while (g_draw_request_thread_running)
    {
        // 60 FPS
        uint32_t milli_second = 1000U / 60U;

        // wait
        {
            struct timespec request = {((time_t)milli_second) / ((time_t)1000), ((long)1000000) * (((long)milli_second) % ((long)1000))};

            struct timespec remain;
            int res_nanosleep;
            while ((-1 == (res_nanosleep = nanosleep(&request, &remain))) && (EINTR == errno))
            {
                assert(remain.tv_nsec > 0 || remain.tv_sec > 0);
                request = remain;
            }
            assert(0 == res_nanosleep);
        }

        // draw request
        {
            uint8_t buf[1] = {7}; // seven is the luck number
            ssize_t res_send;
            while ((-1 == (res_send = send(g_main_thread_looper_draw_callback_fd_write, buf, 1U, 0))) && (EINTR == errno))
            {
                // pthread_yield();
                sched_yield();
            }
            assert(1 == res_send);
        }
    }

    return NULL;
}

static int main_thread_looper_input_queue_callback(int fd, int, void *input_queue_void)
{
    AInputQueue *input_queue = static_cast<AInputQueue *>(input_queue_void);

    AInputEvent *input_event;
    while (AInputQueue_getEvent(input_queue, &input_event) >= 0)
    {
        // The app will be "No response" if we don't call AInputQueue_finishEvent and pass the non-zero value for all events which is not pre-dispatched
        if (0 == AInputQueue_preDispatchEvent(input_queue, input_event))
        {
            int handled = 0;

            switch (AInputEvent_getType(input_event))
            {
            case AINPUT_EVENT_TYPE_MOTION:
            {
                int combined_action_code_and_pointer_index = AMotionEvent_getAction(input_event);

                int action_code = (combined_action_code_and_pointer_index & AMOTION_EVENT_ACTION_MASK);

                switch (action_code)
                {
                case AMOTION_EVENT_ACTION_DOWN:
                case AMOTION_EVENT_ACTION_UP:
                {
                    // TODO:
                    // g_camera.HandleKeyDownMessage(mapped_key);
                    // g_camera.HandleKeyUpMessage(mapped_key);
                }
                break;
                case AMOTION_EVENT_ACTION_POINTER_UP:
                case AMOTION_EVENT_ACTION_POINTER_DOWN:
                {
                    int pointer_index = ((combined_action_code_and_pointer_index & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
                    switch (pointer_index)
                    {
                    case 0:
                    {
                        // TODO:
                        // g_camera.HandleKeyDownMessage(mapped_key);
                        // g_camera.HandleKeyUpMessage(mapped_key);
                    }
                    break;
                    case 1:
                    {
                        float normalized_x = AMotionEvent_getX(input_event, pointer_index) / static_cast<float>(g_window_width);
                        float normalized_y = AMotionEvent_getY(input_event, pointer_index) / static_cast<float>(g_window_height);

                        bool left_button = false;
                        bool middle_button = false;
                        bool right_button = false;

                        g_camera.HandleMouseMoveMessage(normalized_x, normalized_y, left_button, middle_button, right_button);
                    }
                    break;
                    }
                }
                break;
                case AMOTION_EVENT_ACTION_MOVE:
                {
                    size_t pointer_count = AMotionEvent_getPointerCount(input_event);
                    for (size_t pointer_index = 0; pointer_index < 2 && pointer_index < pointer_count; ++pointer_index)
                    {
                        switch (pointer_index)
                        {
                        case 0:
                        {
                            // TODO:
                            // g_camera.HandleKeyDownMessage(mapped_key);
                            // g_camera.HandleKeyUpMessage(mapped_key);
                        }
                        break;
                        case 1:
                        {
                            assert(1 == pointer_index);

                            float normalized_x = AMotionEvent_getX(input_event, pointer_index) / static_cast<float>(g_window_width);
                            float normalized_y = AMotionEvent_getY(input_event, pointer_index) / static_cast<float>(g_window_height);

                            bool left_button = false;
                            bool middle_button = false;
                            bool right_button = true;

                            g_camera.HandleMouseMoveMessage(normalized_x, normalized_y, left_button, middle_button, right_button);
                        }
                        break;
                        }
                    }
                }
                break;
                }
            }
            break;
            }

            AInputQueue_finishEvent(input_queue, input_event, handled);
        }
    }

    return 1;
}
#else

#include <unistd.h>
#include <xcb/xcb.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    // Vulkan Validation Layer
#ifndef NDEBUG
    {
        // We assume that the "VkLayer_khronos_validation.json" is at the same directory of the executable file
        char dir_name[4096];
        ssize_t res_read_link = readlink("/proc/self/exe", dir_name, sizeof(dir_name) / sizeof(dir_name[0]));
        assert(-1 != res_read_link);

        for (int i = (res_read_link - 1); i > 0; --i)
        {
            if (L'/' == dir_name[i])
            {
                dir_name[i] = L'\0';
                break;
            }
        }

        int res_set_env_vk_layer_path = setenv("VK_LAYER_PATH", dir_name, 1);
        assert(0 == res_set_env_vk_layer_path);

        int res_set_env_ld_library_path = setenv("LD_LIBRARY_PATH", dir_name, 1);
        assert(0 == res_set_env_ld_library_path);
    }
#endif

    g_window_width = 1280;
    g_window_height = 720;

    xcb_connection_t *connection = NULL;
    xcb_screen_t *screen = NULL;
    {
        int screen_number;
        connection = xcb_connect(NULL, &screen_number);
        assert(0 == xcb_connection_has_error(connection));

        xcb_setup_t const *setup = xcb_get_setup(connection);

        int i = 0;
        for (xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator(setup); screen_iterator.rem > 0; xcb_screen_next(&screen_iterator))
        {
            if (i == screen_number)
            {
                screen = screen_iterator.data;
                break;
            }
            ++i;
        }
    }

    constexpr uint8_t const depth = 32;
    xcb_visualid_t visual_id = -1;
    {
        for (xcb_depth_iterator_t depth_iterator = xcb_screen_allowed_depths_iterator(screen); depth_iterator.rem > 0; xcb_depth_next(&depth_iterator))
        {
            if (depth == depth_iterator.data->depth)
            {
                for (xcb_visualtype_iterator_t visual_iterator = xcb_depth_visuals_iterator(depth_iterator.data); visual_iterator.rem > 0; xcb_visualtype_next(&visual_iterator))
                {
                    if (XCB_VISUAL_CLASS_TRUE_COLOR == visual_iterator.data->_class)
                    {
                        visual_id = visual_iterator.data->visual_id;
                        break;
                    }
                }
                break;
            }
        }
    }

    xcb_colormap_t colormap = 0;
    {
        colormap = xcb_generate_id(connection);

        xcb_void_cookie_t cookie_create_colormap = xcb_create_colormap_checked(connection, XCB_COLORMAP_ALLOC_NONE, colormap, screen->root, visual_id);

        xcb_generic_error_t *error_create_colormap = xcb_request_check(connection, cookie_create_colormap);
        assert(NULL == error_create_colormap);
    }

    xcb_window_t window = 0;
    {
        window = xcb_generate_id(connection);

        // Both "border pixel" and "colormap" are required when the depth is NOT equal to the root window's.
        uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_BACKING_STORE | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;

        // we need "XCB_EVENT_MASK_BUTTON_PRESS" and "XCB_EVENT_MASK_BUTTON_RELEASE" to use the "state" of the "motion notify" event
        uint32_t value_list[5] = {screen->black_pixel, 0, XCB_BACKING_STORE_NOT_USEFUL, XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_STRUCTURE_NOTIFY, colormap};

        xcb_void_cookie_t cookie_create_window = xcb_create_window_checked(connection, depth, window, screen->root, 0, 0, g_window_width, g_window_height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, visual_id, value_mask, value_list);

        xcb_generic_error_t *error_create_window = xcb_request_check(connection, cookie_create_window);
        assert(NULL == error_create_window);
    }

    xcb_atom_t atom_wm_protocols = 0;
    xcb_atom_t atom_wm_delete_window = 0;
    {
        xcb_intern_atom_cookie_t cookie_wm_protocols = xcb_intern_atom(connection, 0, 12U, "WM_PROTOCOLS");

        xcb_intern_atom_cookie_t cookie_wm_delete_window = xcb_intern_atom(connection, 0, 16U, "WM_DELETE_WINDOW");

        xcb_generic_error_t *error_intern_atom_reply_wm_protocols;
        xcb_intern_atom_reply_t *reply_wm_protocols = xcb_intern_atom_reply(connection, cookie_wm_protocols, &error_intern_atom_reply_wm_protocols);
        assert(NULL == error_intern_atom_reply_wm_protocols);
        atom_wm_protocols = reply_wm_protocols->atom;
        free(error_intern_atom_reply_wm_protocols);

        xcb_generic_error_t *error_intern_atom_reply_wm_delete_window;
        xcb_intern_atom_reply_t *reply_wm_delete_window = xcb_intern_atom_reply(connection, cookie_wm_delete_window, &error_intern_atom_reply_wm_delete_window);
        assert(NULL == error_intern_atom_reply_wm_delete_window);
        atom_wm_delete_window = reply_wm_delete_window->atom;
        free(error_intern_atom_reply_wm_delete_window);
    }

    {
        xcb_void_cookie_t cookie_change_property_wm_name = xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8 * sizeof(uint8_t), 12, "Path Tracing");

        xcb_void_cookie_t cookie_change_property_wm_protocols_delete_window = xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE, window, atom_wm_protocols, XCB_ATOM_ATOM, 8 * sizeof(uint32_t), sizeof(xcb_atom_t) / sizeof(uint32_t), &atom_wm_delete_window);

        xcb_generic_error_t *error_change_property_net_wm_name = xcb_request_check(connection, cookie_change_property_wm_name);
        assert(NULL == error_change_property_net_wm_name);

        xcb_generic_error_t *error_change_property_wm_protocols_delete_window = xcb_request_check(connection, cookie_change_property_wm_protocols_delete_window);
        assert(NULL == error_change_property_wm_protocols_delete_window);
    }

    {
        struct brx_xcb_connection_T
        {
            xcb_connection_t *m_connection;
            xcb_visualid_t m_visual_id;
        };
        brx_xcb_connection_T brx_xcb_connection = {
            connection,
            visual_id};

        g_renderer = renderer_init(&brx_xcb_connection);

        struct brx_xcb_window_T
        {
            xcb_connection_t *m_connection;
            xcb_window_t m_window;
        };

        brx_xcb_window_T brx_xcb_window = {
            connection,
            window};

        renderer_attach_window(g_renderer, &brx_xcb_window);
    }

    {
        xcb_void_cookie_t cookie_map_window = xcb_map_window_checked(connection, window);

        xcb_generic_error_t *error_map_window = xcb_request_check(connection, cookie_map_window);
        assert(NULL == error_map_window);
    }

    bool quit = false;
    while (!quit)
    {
        // WSI Event
        {
            xcb_generic_event_t *event;
            while ((event = xcb_poll_for_event(connection)) != NULL)
            {
                // The most significant bit(uint8_t(0X80)) in this code is set if the event was generated from a SendEvent request.
                // https://www.x.org/releases/current/doc/xproto/x11protocol.html#event_format
                switch (event->response_type & (~uint8_t(0X80)))
                {
                case XCB_KEY_PRESS:
                {
                    assert(XCB_KEY_PRESS == (event->response_type & (~uint8_t(0X80))));

                    xcb_key_press_event_t const *const key_press = reinterpret_cast<xcb_key_press_event_t *>(event);

                    // evdev keycode
                    // https://gitlab.freedesktop.org/xkeyboard-config/xkeyboard-config/-/blob/master/keycodes/evdev
                    // Q
                    constexpr xcb_keycode_t const AD01 = 24;
                    // W
                    constexpr xcb_keycode_t const AD02 = 25;
                    // E
                    constexpr xcb_keycode_t const AD03 = 26;
                    // A
                    constexpr xcb_keycode_t const AC01 = 38;
                    // S
                    constexpr xcb_keycode_t const AC02 = 39;
                    // D
                    constexpr xcb_keycode_t const AC03 = 40;

                    D3DUtil_CameraKeys mapped_key;
                    switch (key_press->detail)
                    {
                    case AD02:
                    {
                        mapped_key = CAM_MOVE_FORWARD;
                    }
                    break;
                    case AC02:
                    {
                        mapped_key = CAM_MOVE_BACKWARD;
                    }
                    break;
                    case AC01:
                    {
                        mapped_key = CAM_STRAFE_LEFT;
                    }
                    break;
                    case AC03:
                    {
                        mapped_key = CAM_STRAFE_RIGHT;
                    }
                    break;
                    case AD01:
                    {
                        mapped_key = CAM_MOVE_UP;
                    }
                    break;
                    case AD03:
                    {
                        mapped_key = CAM_MOVE_DOWN;
                    }
                    break;
                    default:
                    {
                        mapped_key = CAM_UNKNOWN;
                    }
                    }

                    g_camera.HandleKeyDownMessage(mapped_key);
                }
                break;
                case XCB_KEY_RELEASE:
                {
                    assert(XCB_KEY_RELEASE == (event->response_type & (~uint8_t(0X80))));

                    xcb_key_release_event_t const *const key_release = reinterpret_cast<xcb_key_release_event_t *>(event);

                    // evdev keycode
                    // https://gitlab.freedesktop.org/xkeyboard-config/xkeyboard-config/-/blob/master/keycodes/evdev
                    // Q
                    constexpr xcb_keycode_t const AD01 = 24;
                    // W
                    constexpr xcb_keycode_t const AD02 = 25;
                    // E
                    constexpr xcb_keycode_t const AD03 = 26;
                    // A
                    constexpr xcb_keycode_t const AC01 = 38;
                    // S
                    constexpr xcb_keycode_t const AC02 = 39;
                    // D
                    constexpr xcb_keycode_t const AC03 = 40;

                    D3DUtil_CameraKeys mapped_key;
                    switch (key_release->detail)
                    {
                    case AD02:
                    {
                        mapped_key = CAM_MOVE_FORWARD;
                    }
                    break;
                    case AC02:
                    {
                        mapped_key = CAM_MOVE_BACKWARD;
                    }
                    break;
                    case AC01:
                    {
                        mapped_key = CAM_STRAFE_LEFT;
                    }
                    break;
                    case AC03:
                    {
                        mapped_key = CAM_STRAFE_RIGHT;
                    }
                    break;
                    case AD01:
                    {
                        mapped_key = CAM_MOVE_UP;
                    }
                    break;
                    case AD03:
                    {
                        mapped_key = CAM_MOVE_DOWN;
                    }
                    break;
                    default:
                    {
                        mapped_key = CAM_UNKNOWN;
                    }
                    }

                    g_camera.HandleKeyUpMessage(mapped_key);
                }
                break;
                case XCB_MOTION_NOTIFY:
                {
                    assert(XCB_MOTION_NOTIFY == (event->response_type & (~uint8_t(0X80))));

                    xcb_motion_notify_event_t const *const motion_notify = reinterpret_cast<xcb_motion_notify_event_t *>(event);

                    float normalized_x = static_cast<float>(motion_notify->event_x) / g_window_width;
                    float normalized_y = static_cast<float>(motion_notify->event_y) / g_window_height;

                    bool left_button = (0U != (motion_notify->state & XCB_EVENT_MASK_BUTTON_1_MOTION));
                    bool middle_button = (0U != (motion_notify->state & XCB_EVENT_MASK_BUTTON_2_MOTION));
                    bool right_button = (0U != (motion_notify->state & XCB_EVENT_MASK_BUTTON_3_MOTION));

                    g_camera.HandleMouseMoveMessage(normalized_x, normalized_y, left_button, middle_button, right_button);
                }
                break;
                case XCB_CONFIGURE_NOTIFY:
                {
                    assert(XCB_CONFIGURE_NOTIFY == (event->response_type & (~uint8_t(0X80))));

                    xcb_configure_notify_event_t *const configure_notify = reinterpret_cast<xcb_configure_notify_event_t *>(event);

                    if (g_window_width != configure_notify->width || g_window_height != configure_notify->height)
                    {
                        renderer_on_window_resize(g_renderer);
                        g_window_width = configure_notify->width;
                        g_window_height = configure_notify->height;
                    }
                }
                break;
                case XCB_CLIENT_MESSAGE:
                {
                    assert(XCB_CLIENT_MESSAGE == (event->response_type & (~uint8_t(0X80))));

                    xcb_client_message_event_t *const client_message_event = reinterpret_cast<xcb_client_message_event_t *>(event);
                    assert(client_message_event->type == atom_wm_protocols && client_message_event->data.data32[0] == atom_wm_delete_window && client_message_event->window == window);

                    quit = true;
                }
                break;
                case 0:
                {
                    assert(0 == (event->response_type & (~uint8_t(0X80))));

                    xcb_generic_error_t *error = reinterpret_cast<xcb_generic_error_t *>(event);

                    printf("Error Code: %d Major Code: %d", static_cast<int>(error->error_code), static_cast<int>(error->major_code));
                }
                break;
                }

                free(event);
            }
        }

        // Render
        renderer_draw(g_renderer);
    }

    {
        xcb_void_cookie_t cookie_free_colormap = xcb_free_colormap_checked(connection, colormap);

        xcb_generic_error_t *error_free_colormap = xcb_request_check(connection, cookie_free_colormap);
        assert(NULL == error_free_colormap);
    }

    xcb_disconnect(connection);

    renderer_dettach_window(g_renderer);

    renderer_destroy(g_renderer);

    return 0;
}

#endif
#else
#error Unknown Platform
#endif

#elif defined(_MSC_VER)

#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN 1
#include <sdkddkver.h>
#include <Windows.h>
#include <windowsx.h>

static LRESULT CALLBACK wnd_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    // Vulkan Validation Layer
#ifndef NDEBUG
    {
        // We assume that the "VkLayer_khronos_validation.json" is at the same directory of the executable file
        WCHAR file_name[4096];
        DWORD res_get_file_name = GetModuleFileNameW(NULL, file_name, sizeof(file_name) / sizeof(file_name[0]));
        assert(0U != res_get_file_name);

        for (int i = (res_get_file_name - 1); i > 0; --i)
        {
            if (L'\\' == file_name[i])
            {
                file_name[i] = L'\0';
                break;
            }
        }

        BOOL res_set_environment_variable = SetEnvironmentVariableW(L"VK_LAYER_PATH", file_name);
        assert(FALSE != res_set_environment_variable);
    }
#endif

    // Initialize
    {
        g_window_width = 1280;
        g_window_height = 720;

        ATOM hWndCls;
        {
            WNDCLASSEXW Desc = {
                sizeof(WNDCLASSEX),
                CS_OWNDC,
                wnd_proc,
                0,
                0,
                hInstance,
                LoadIconW(NULL, IDI_APPLICATION),
                LoadCursorW(NULL, IDC_ARROW),
                (HBRUSH)(COLOR_WINDOW + 1),
                NULL,
                L"Path-Tracing:0XFFFFFFFF",
                LoadIconW(NULL, IDI_APPLICATION),
            };
            hWndCls = RegisterClassExW(&Desc);
        }

        HWND hWnd;
        {
            HWND hDesktop = GetDesktopWindow();
            HMONITOR hMonitor = MonitorFromWindow(hDesktop, MONITOR_DEFAULTTONEAREST);
            MONITORINFOEXW MonitorInfo;
            MonitorInfo.cbSize = sizeof(MONITORINFOEXW);
            GetMonitorInfoW(hMonitor, &MonitorInfo);

            constexpr DWORD const dw_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
            constexpr DWORD const dw_ex_style = WS_EX_APPWINDOW;

            RECT rect = {(MonitorInfo.rcWork.left + MonitorInfo.rcWork.right) / 2 - g_window_width / 2,
                         (MonitorInfo.rcWork.bottom + MonitorInfo.rcWork.top) / 2 - g_window_height / 2,
                         (MonitorInfo.rcWork.left + MonitorInfo.rcWork.right) / 2 + g_window_width / 2,
                         (MonitorInfo.rcWork.bottom + MonitorInfo.rcWork.top) / 2 + g_window_height / 2};
            AdjustWindowRectEx(&rect, dw_style, FALSE, dw_ex_style);

            hWnd = CreateWindowExW(dw_ex_style, MAKEINTATOM(hWndCls), L"Path Tracing", dw_style, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, hDesktop, NULL, hInstance, NULL);
        }

        g_renderer = renderer_init(NULL);

        renderer_attach_window(g_renderer, hWnd);

        ShowWindow(hWnd, SW_SHOWDEFAULT);
        {
            BOOL result_update_window = UpdateWindow(hWnd);
            assert(FALSE != result_update_window);
        }
    }

    // Run
    {
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // Destroy
    {
        renderer_dettach_window(g_renderer);

        renderer_destroy(g_renderer);
    }

    return 0;
}

static LRESULT CALLBACK wnd_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    }
        return 0;
    case WM_PAINT:
    {
        if (g_window_width > 0 && g_window_height > 0)
        {
            renderer_draw(g_renderer);
        }
    }
        return 0;
    case WM_SIZE:
    {
        WORD const new_width = LOWORD(lParam);
        WORD const new_height = HIWORD(lParam);

        if (g_window_width != new_width || g_window_height != new_height)
        {
            if (new_width > 0 && new_height > 0)
            {
                renderer_on_window_resize(g_renderer);
            }
            g_window_width = new_width;
            g_window_height = new_height;
        }
    }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_KEYDOWN:
    {
        D3DUtil_CameraKeys mappedKey;
        switch (wParam)
        {
        case 'W':
        {
            mappedKey = CAM_MOVE_FORWARD;
        }
        break;
        case 'S':
        {
            mappedKey = CAM_MOVE_BACKWARD;
        }
        break;
        case 'A':
        {
            mappedKey = CAM_STRAFE_LEFT;
        }
        break;
        case 'D':
        {
            mappedKey = CAM_STRAFE_RIGHT;
        }
        break;
        case 'Q':
        {
            mappedKey = CAM_MOVE_UP;
        }
        break;
        case 'E':
        {
            mappedKey = CAM_MOVE_DOWN;
        }
        break;
        default:
        {
            mappedKey = CAM_UNKNOWN;
        }
        };

        g_camera.HandleKeyDownMessage(mappedKey);
    }
        return 0;
    case WM_KEYUP:
    {
        D3DUtil_CameraKeys mappedKey;
        switch (wParam)
        {
        case 'W':
        {
            mappedKey = CAM_MOVE_FORWARD;
        }
        break;
        case 'S':
        {
            mappedKey = CAM_MOVE_BACKWARD;
        }
        break;
        case 'A':
        {
            mappedKey = CAM_STRAFE_LEFT;
        }
        break;
        case 'D':
        {
            mappedKey = CAM_STRAFE_RIGHT;
        }
        break;
        case 'Q':
        {
            mappedKey = CAM_MOVE_UP;
        }
        break;
        case 'E':
        {
            mappedKey = CAM_MOVE_DOWN;
        }
        break;
        default:
        {
            mappedKey = CAM_UNKNOWN;
        }
        };

        g_camera.HandleKeyUpMessage(mappedKey);
    }
        return 0;
    case WM_MOUSEMOVE:
    {
        float normalizedX = static_cast<float>(static_cast<double>(GET_X_LPARAM(lParam)) / static_cast<double>(g_window_width));
        float normalizedY = static_cast<float>(static_cast<double>(GET_Y_LPARAM(lParam)) / static_cast<double>(g_window_height));

        bool leftButton = (0U != (wParam & MK_LBUTTON));
        bool middleButton = (0U != (wParam & MK_MBUTTON));
        bool rightButton = (0U != (wParam & MK_RBUTTON));

        g_camera.HandleMouseMoveMessage(normalizedX, normalizedY, leftButton, middleButton, rightButton);
    }
        return 0;
    default:
        return DefWindowProc(hWnd, Msg, wParam, lParam);
    }
}

#else
#error Unknown Compiler
#endif