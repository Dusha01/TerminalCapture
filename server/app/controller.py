import socket
import threading
import json
import os
import front
import ssl

CONTROLLER_IP = "127.0.0.1"
CONTROLLER_PORT = 12345
BOT_DATA_FILE = "data/bots.json"

bots = {}
client_sockets = {}
bot_threads = {}
server_running = True


def load_bots_from_json():
    global bots
    if os.path.exists(BOT_DATA_FILE):
        with open(BOT_DATA_FILE, "r") as f:
            try:
                data = json.load(f)
                bots = data
            except json.JSONDecodeError:
                print("Ошибка чтения bots.json. Файл может быть поврежден.")
                bots = {}
    else:
        bots = {}


def save_bots_to_json():
    with open(BOT_DATA_FILE, "w") as f:
        json.dump(bots, f, indent=4)


def handle_client(client_socket, client_address):
    global bots, client_sockets, bot_threads, server_running

    bot_id = None
    try:
        data = client_socket.recv(1024).decode()
        if not data:
            print(f"Клиент {client_address} отключился до регистрации.")
            return

        try:
            message = json.loads(data)
            bot_id = message.get("bot_id")
            action = message.get("action")

            if action == "register":
                print(f"Бот {bot_id} зарегистрирован.")
                bots[bot_id] = {"address": client_address}
                client_sockets[bot_id] = client_socket
                save_bots_to_json()
                client_socket.sendall("Регистрация прошла успешно".encode())
            else:
                print(f"Неожиданное действие от {client_address}: {action}")
                return

        except json.JSONDecodeError:
            print(f"Неверный формат JSON от {client_address}: {data}")
            return

        bot_thread = threading.Thread(target=bot_interaction_loop, args=(bot_id,))
        bot_threads[bot_id] = bot_thread
        bot_thread.daemon = True
        bot_thread.start()

        while server_running:
            try:
                data = client_socket.recv(1024)
                if not data:
                    break
                print(f"Бот {bot_id} отправил: {data.decode()}")

            except ConnectionResetError:
                print(f"Бот {bot_id} внезапно отключился.")
                break
            except Exception as e:
                print(f"Ошибка при получении данных от бота {bot_id}: {e}")
                break

    except Exception as e:
        print(f"Ошибка при обработке клиента {client_address}: {e}")
    finally:
        print(f"Соединение с {client_address} закрыто.")
        client_socket.close()
        if bot_id in bots:
            del bots[bot_id]
            save_bots_to_json()
        if bot_id in client_sockets:
            del client_sockets[bot_id]
        if bot_id in bot_threads:
            del bot_threads[bot_id]


def bot_interaction_loop(bot_id):
    global client_sockets, bots

    while True:
        command = input(
            f"Введите команду для бота {bot_id} (или 'break' чтобы вернуться в меню, 'exit' чтобы отключиться, 'menu' - перейти в меню сервера): "
        )

        if command.lower() == "break":
            print(f"Завершение ввода команд для бота {bot_id}.")
            break

        elif command.lower() == "exit":
            print(f"Отключение бота {bot_id}")
            try:
                client_sockets[bot_id].close()
            except (KeyError, OSError) as e:
                print(f"Ошибка при закрытии сокета для бота {bot_id}: {e}")
            break

        elif command.lower() == "menu":
            server_commands()
            print(f"Возвращаемся к управлению ботом {bot_id}")


        elif bot_id in client_sockets:
            try:
                client_sockets[bot_id].sendall(command.encode())
            except (BrokenPipeError, ConnectionResetError):
                print(f"Бот {bot_id} отключился.")
                if bot_id in bots:
                    del bots[bot_id]
                del client_sockets[bot_id]
                save_bots_to_json()
                break
            except Exception as e:
                print(f"Ошибка при отправке команды боту {bot_id}: {e}")
                break

        else:
            print(f"Сокет для бота {bot_id} не найден.")
            break


def server_commands():
    global bots, server_running
    while True:
        command = input("Введите команду сервера (list, exit, start): ").lower()
        if command == "list":
            front.list_bots(bots)
        elif command == "exit":
            print("Завершение работы сервера...")
            server_running = False
            break
        elif command == "start":
            print("Возвращаемся к работе с ботами")
            return
        else:
            print("Неверная команда сервера.")


def main():
    global bots, client_sockets, bot_threads, server_running

    load_bots_from_json()

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((CONTROLLER_IP, CONTROLLER_PORT))
    server_socket.listen(5)

    print(f"Контроллер запущен на {CONTROLLER_IP}:{CONTROLLER_PORT}")

    try:
        while server_running:
            client_socket, client_address = server_socket.accept()
            print(f"Соединение от {client_address}")
            client_thread = threading.Thread(
                target=handle_client, args=(client_socket, client_address)
            )
            client_thread.daemon = True
            client_thread.start()

    except KeyboardInterrupt:
        print("\nКонтроллер завершает работу...")
        server_running = False
    finally:
        for bot_id, sock in client_sockets.items():
            try:
                sock.close()
            except OSError:
                pass
        server_socket.close()
        print("Сервер остановлен.")


if __name__ == "__main__":
    front.picture()
    if not os.path.exists("data"):
        os.makedirs("data")

    load_bots_from_json()

    while True:
        selected_option = front.main_menu(bots)

        if selected_option == "start":
            print("Запуск основной функциональности (прослушивание подключений ботов)...")
            main()
            break

        elif selected_option == "server_commands":
            server_commands()


        elif selected_option is None:
            print("Выход из программы.")
            break

        elif selected_option.startswith("bot_"):
            selected_bot_id = selected_option.split("_")[1]
            if selected_bot_id not in client_sockets:
                print(f"Бот {selected_bot_id} в данный момент не подключен.")
                continue

            bot_interaction_loop(
                selected_bot_id
            )
        elif selected_option == "start":
            print("Начало работы с ботами, ожидание подключения")
            main()
            break

        else:
            print("Неизвестная команда")
            break