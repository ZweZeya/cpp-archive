#include <thread>
#include <iostream>
#include <semaphore>
#include <atomic>

void ping_pong1(int times, std::ostream& os = std::cout) {
    std::binary_semaphore ping_turn{1}, pong_turn{0};

    std::jthread pong([&] {
        for (int i = 0; i < times; i++) {
            pong_turn.acquire();
            os << "Pong " << i << '\n';
            ping_turn.release();
        }
    });

    for (int i = 0; i < times; i++) {
        ping_turn.acquire();
        os << "Ping " << i << '\n';
        pong_turn.release();
    }
}

void ping_pong2(int times, std::ostream& os = std::cout) {
    std::atomic<int> turn{0};

    auto player = [&] (int me, auto&& action) {
        for (int i = 0; i < times; i++) {
            int t;
            while ((t = turn.load(std::memory_order_acquire)) != me) 
                turn.wait(t, std::memory_order_acquire);
            action(i);
            turn.store(1 - me, std::memory_order_release);
            turn.notify_one();
        }
    };

    std::jthread pong(player, 1, [&] (int i) {
        os << "Pong " << i << '\n';
    });

    player(0, [&] (int i) {
        os << "Ping " << i << '\n';
    });
}

int main() {
    ping_pong1(5);
    ping_pong2(5);
}