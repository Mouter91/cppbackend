#pragma once
#include <boost/asio/dispatch.hpp>
#include <stdexcept>
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
       auto sausage = store_.GetSausage();
       auto bread = store_.GetBread();

       auto check_ready = [this, bread, sausage]() {
           if (bread->IsCooked() && sausage->IsCooked()) {
               try {
                   net::dispatch(io_, [hotdog = std::make_shared<HotDog>(++id_counter, sausage, bread)] (){});
               } catch (const std::runtime_error e) {
                   throw e;
               }
           }
       };

       net::post(io_, [this, sausage, check_ready]() {
               sausage->StartFry(*gas_cooker_, [&, sausage]() {
                       sausage->StopFry();
                       check_ready();
                       });
               });

       net::post(io_, [this, bread, check_ready]() {
               bread->StartBake(*gas_cooker_, [&, bread]() {
                       bread->StopBaking();
                       check_ready();
                       });
               });
    }

private:
    int id_counter = 0;
    net::io_context& io_;
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
