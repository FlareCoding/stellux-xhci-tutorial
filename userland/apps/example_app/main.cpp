#include <ipc/mq.h>
#include <serial/serial.h>
#include <time/time.h>
#include <process/process.h>
#include <stella_user.h>

int main() {
    if (!stella_ui::connect_to_compositor()) {
        serial::printf("[EXAMPLE_APP] Failed to connect to compositor\n");
        return -1;
    }

    serial::printf("[EXAMPLE_APP] Connected to compositor!\n");
    sleep(2);

    if (!stella_ui::create_window(400, 300, "Example App")) {
        serial::printf("[EXAMPLE_APP] Failed to create a window\n");
        return -1;
    }

    kstl::shared_ptr<stella_ui::canvas> canvas;
    if (!stella_ui::request_map_window_canvas(canvas)) {
        serial::printf("[EXAMPLE_APP] Failed to map window canvas\n");
        return -1;
    }

    canvas->set_background_color(stella_ui::color::dark_gray.to_argb());

    char pid_str[64] = { 0 };
    sprintf(pid_str, 63, "pid: %u", current->pid);

    canvas->clear();
    canvas->draw_string(20, 20, pid_str, stella_ui::color::green.to_argb());

    while (true) {
        canvas->clear();
        canvas->draw_string(20, 20, pid_str, stella_ui::color::green.to_argb());
        sched::yield();
    }

    return 0;
}
