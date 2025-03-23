import socket
import threading
import json
import os
import front

CONTROLLER_IP = "127.0.0.1"
CONTROLLER_PORT = 12345
BOT_DATA_FILE = "data/bots.json"

bots = {}
client_sockets = {} 

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
        bot_id = None
        data = client_socket.recv(1024).decode()
        if not data:
            print(f"Client {client_address} disconnected before registration.")
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
                client_socket.sendall("Регистрация успешна".encode())
            else:
                print(f"Unexpected action from {client_address}: {action}")
                return
            
        except json.JSONDecodeError:
            print(f"Неверный формат JSON от {client_address}: {data}")
            return

        while True:
            command = input(f"Command для бота {bot_id}: ")
            if command.lower() == "exit":
                print(f"Отключение бота {bot_id}")
                break

            try:
                if bot_id in client_sockets:
                    client_sockets[bot_id].sendall(command.encode())
                else:
                    print(f"Socket for bot {bot_id} not found.")
                    break
            except (BrokenPipeError, ConnectionResetError):
                print(f"Бот {bot_id} отключился.")
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


def main():
    load_bots_from_json()

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((CONTROLLER_IP, CONTROLLER_PORT))
    server_socket.listen(5)

    print(f"Controller run {CONTROLLER_IP}:{CONTROLLER_PORT}")

    try:
        while True:
            client_socket, client_address = server_socket.accept()
            print(f"Подключение от {client_address}")
            client_thread = threading.Thread(target=handle_client, args=(client_socket, client_address))
            client_thread.daemon = True
            client_thread.start()

    except KeyboardInterrupt:
        print("\nController off...")
    finally:
        server_socket.close()


if __name__ == "__main__":
    front.picture()
    if not os.path.exists("data"):
        os.makedirs("data")
    
    load_bots_from_json()

    while True:
        selected_bot_id = front.main_menu(bots)
        if selected_bot_id is None:
            break

        while True:
            command = input(f"Введите команду для бота {selected_bot_id} (или 'exit' для выхода): ")
            if command.lower() == "exit":
                break

            if selected_bot_id in client_sockets:
                try:
                    client_sockets[selected_bot_id].sendall(command.encode())
                except (BrokenPipeError, ConnectionResetError):
                    print(f"Бот {selected_bot_id} отключился.")
                    del bots[selected_bot_id]
                    del client_sockets[selected_bot_id]
                    save_bots_to_json()
                    break
            else:
                print(f"Соединение с ботом {selected_bot_id} потеряно.")
                break