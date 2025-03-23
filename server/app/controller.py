import socket
import threading
import json
import os

CONTROLLER_IP = "127.0.0.1"
CONTROLLER_PORT = 12345
BOT_DATA_FILE = "data/bots.json"

bots = {}


def load_bots_from_json():
    global bots
    if os.path.exists(BOT_DATA_FILE):
        with open(BOT_DATA_FILE, "r") as f:
            try:
                data = json.load(f)
                bots = data
            except json.JSONDecodeError:
                print("Ошибка при чтении bots.json. Файл может быть поврежден.")
                bots = {}
    else:
        bots = {}


def save_bots_to_json():
    with open(BOT_DATA_FILE, "w") as f:
        json.dump(bots, f, indent=4)


def handle_client(client_socket, client_address):
    try:
        while True:
            data = client_socket.recv(1024).decode()
            if not data:
                break

            try:
                message = json.loads(data)
                bot_id = message.get("bot_id")
                action = message.get("action")

                if action == "register":
                    print(f"Бот {bot_id} зарегистрирован.")
                    bots[bot_id] = {"address": client_address}
                    save_bots_to_json()
                    client_socket.sendall("Регистрация успешна".encode())

                elif action == "get_command":
                    command = input(f"Введите команду для бота {bot_id}: ")
                    client_socket.sendall(command.encode())
            except json.JSONDecodeError:
                print("Неверный формат JSON от клиента.")

    except Exception as e:
        print(f"Ошибка при обработке клиента: {e}")
    finally:
        print(f"Соединение с {client_address} закрыто.")
        client_socket.close()
        for bot_id, bot_data in list(bots.items()):
            if bot_data["address"] == client_address:
                del bots[bot_id]
                save_bots_to_json()
                break


def main():
    load_bots_from_json()

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((CONTROLLER_IP, CONTROLLER_PORT))
    server_socket.listen(5)

    print(f"Контроллер запущен, ожидает подключения ботов на {CONTROLLER_IP}:{CONTROLLER_PORT}")

    try:
        while True:
            client_socket, client_address = server_socket.accept()
            print(f"Подключение от {client_address}")
            client_thread = threading.Thread(target=handle_client, args=(client_socket, client_address))
            client_thread.start()

    except KeyboardInterrupt:
        print("\nЗавершение работы контроллера...")
    finally:
        server_socket.close()


if __name__ == "__main__":
    if not os.path.exists("data"):
        os.makedirs("data")
    main()