# 🚀 Учебный HTTP-сервер на Boost.Beast

Этот проект — простой учебный HTTP-сервер, написанный на C++ с использованием библиотек **Boost.Asio** и **Boost.Beast**. Он демонстрирует основы асинхронного сетевого программирования и обработки HTTP-запросов на низком уровне.

---

## 📚 Цель проекта

> Научиться создавать и настраивать простой HTTP-сервер с поддержкой запросов и маршрутизации на основе C++ и Boost.

---


---


---

## 🧰 Стек технологий

![C++17](https://img.shields.io/badge/C++17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![Boost.Asio](https://img.shields.io/badge/Boost.Asio-005C9C?style=for-the-badge&logo=boost&logoColor=white)
![Boost.Beast](https://img.shields.io/badge/Boost.Beast-009FE3?style=for-the-badge&logo=boost&logoColor=white)
![TCP/IP](https://img.shields.io/badge/TCP/IP-678FFF?style=for-the-badge&logo=network-wired&logoColor=white)


## ⚙️ Сборка

Компилируем с помощью `g++`:

```bash
g++ -std=c++17 main.cpp -o http_server -lboost_system -lpthread
```

---

## 🚀 Запуск

```bash
./http_server
```

По умолчанию сервер начинает прослушивать TCP-порт и принимает HTTP-запросы.

---

## 💡 Пример использования

После запуска сервера отправьте HTTP-запрос с помощью браузера или `curl`:

```bash
curl http://localhost:8080/
```

В ответ вернётся HTTP-страница с определённым содержанием и заголовками.

---

## 🔍 Особенности

- 📡 Асинхронная работа с сокетами (Boost.Asio)
- 🌐 Поддержка HTTP/1.1 (Boost.Beast)
- 📄 Обработка `Content-Type` и HTTP-методов
- 🧱 Структура кода, подходящая для расширения

---

## 📁 Структура проекта

```
.
├── main.cpp      # Основная реализация сервера
└── README.md     # Документация проекта
```

---

## 🛠 Идеи для развития

- Поддержка HTTPS
- Обработка разных HTTP-методов (GET, POST)
- Простая система маршрутов
- Логирование запросов
- Обслуживание статических файлов

---

🎓 Проект предназначен для обучения и экспериментов. Отличный способ начать работу с сетевым программированием на C++!
