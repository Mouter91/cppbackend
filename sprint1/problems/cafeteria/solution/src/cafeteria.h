#pragma once
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <stdexcept>
#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Cafeteria : public std::enable_shared_from_this<Cafeteria> {
public:
    explicit Cafeteria(net::io_context& io)
        : io_(io),
          strand_(net::make_strand(io)) {
    }

    void OrderHotDog(HotDogHandler handler) {
        auto self = shared_from_this();
        
        auto sausage = store_.GetSausage();
        auto bread = store_.GetBread();

        auto check_ready = [self, bread, sausage, handler]() {
            if (bread->IsCooked() && sausage->IsCooked()) {
                try {
                    auto hotdog = std::make_shared<HotDog>(++self->id_counter_, sausage, bread);
                    net::dispatch(self->io_, [handler, hotdog]() {
                        handler(Result<HotDog>(*hotdog));
                    });
                } catch (...) {
                    net::dispatch(self->io_, [handler]() {
                        handler(Result<HotDog>::FromCurrentException());
                    });
                }
            }
        };

        net::post(strand_, [self, sausage, check_ready]() {
            sausage->StartFry(*self->gas_cooker_, [sausage, check_ready]() {
                sausage->StopFry();
                check_ready();
            });
        });

        net::post(strand_, [self, bread, check_ready]() {
            bread->StartBake(*self->gas_cooker_, [bread, check_ready]() {
                bread->StopBaking();
                check_ready();
            });
        });
    }

private:
    std::atomic<int> id_counter_ = 0;
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
