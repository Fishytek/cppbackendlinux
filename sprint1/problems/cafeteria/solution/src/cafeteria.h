#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <atomic>

#include "hotdog.h"
#include "result.h"
#include "ingredients.h"

namespace net = boost::asio;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    void OrderHotDog(HotDogHandler handler) {
        auto bread = store_.GetBread();
        auto sausage = store_.GetSausage();
        
        struct OrderState {
            std::shared_ptr<Bread> bread;
            std::shared_ptr<Sausage> sausage;
            std::atomic<bool> bread_done{false};
            std::atomic<bool> sausage_done{false};
            HotDogHandler handler;
            std::shared_ptr<net::steady_timer> bread_timer;
            std::shared_ptr<net::steady_timer> sausage_timer;
            net::strand<net::io_context::executor_type> strand;
            
            OrderState(net::io_context& io) 
                : strand(net::make_strand(io)) {
            }
        };
        
        auto state = std::make_shared<OrderState>(io_);
        state->bread = std::move(bread);
        state->sausage = std::move(sausage);
        state->handler = std::move(handler);
        
        auto check_complete = [state, this] {
            // Проверяем завершение внутри strand для атомарности
            net::dispatch(state->strand, [state, this] {
                if (state->bread_done.load() && state->sausage_done.load()) {
                    try {
                        int hotdog_id = ++next_hotdog_id_;
                        HotDog hot_dog(hotdog_id, state->sausage, state->bread);
                        state->handler(Result<HotDog>{std::move(hot_dog)});
                    } catch (const std::exception& e) {
                        state->handler(Result<HotDog>{std::make_exception_ptr(e)});
                    }
                }
            });
        };
        
        // Запускаем приготовление хлеба
        state->bread->StartBake(*gas_cooker_, [state, check_complete, this] {
            state->bread_timer = std::make_shared<net::steady_timer>(io_);
            state->bread_timer->expires_after(std::chrono::milliseconds(1000));
            state->bread_timer->async_wait([state, check_complete](const boost::system::error_code& ec) {
                if (!ec) {
                    state->bread->StopBaking();
                    state->bread_done.store(true);
                    check_complete();
                }
            });
        });
        
        // Запускаем жарку сосиски
        state->sausage->StartFry(*gas_cooker_, [state, check_complete, this] {
            state->sausage_timer = std::make_shared<net::steady_timer>(io_);
            state->sausage_timer->expires_after(std::chrono::milliseconds(1500));
            state->sausage_timer->async_wait([state, check_complete](const boost::system::error_code& ec) {
                if (!ec) {
                    state->sausage->StopFry();
                    state->sausage_done.store(true);
                    check_complete();
                }
            });
        });
    }

private:
    net::io_context& io_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
    static std::atomic<int> next_hotdog_id_;
};

