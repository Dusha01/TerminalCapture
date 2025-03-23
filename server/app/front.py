# front.py
def picture():
    print('DDDDD      U     U     SSSS     H     H      A')
    print('D    D     U     U    S    S    H     H     A A')
    print('D     D    U     U   S          H     H    A   A')
    print('D     D    U     U    SSSS      HHHHHHH   AAAAAAA')
    print('D     D    U     U        S     H     H  A       A')
    print('D    D     U     U   S    S     H     H  A       A')
    print('DDDDD       UUUUU     SSSS      H     H  A       A')
    print(' ')
    print(' ')

def display_help():
    print("Доступные команды:")
    print("  - list: Показать список зарегистрированных ботов с IP, ID.")
    print("  - select <bot_id>: Выбрать бота для взаимодействия.")
    print("  - exit: Выход из программы.")
    print("  - help: Показать это сообщение.")

def list_bots(bots):
    if not bots:
        print("Нет зарегистрированных ботов.")
        return

    print("Список зарегистрированных ботов:")
    print("-----------------------------------")
    print("| ID       | IP-адрес          |")
    print("-----------------------------------")
    for bot_id, bot_info in bots.items():
        print(f"| {bot_id:8} | {bot_info['address'][0]:18} |")
    print("-----------------------------------")

def select_bot(bots):
    bot_id = input("Введите ID бота для выбора: ")
    if bot_id in bots:
        print(f"Выбран бот с ID: {bot_id}")
        return bot_id
    else:
        print(f"Бот с ID {bot_id} не найден.")
        return None

def main_menu(bots):
    while True:
        command = input("Введите команду (или 'help' для списка команд): ").lower()

        if command == "help":
            display_help()
        elif command == "list":
            list_bots(bots)
        elif command.startswith("select"):
            try:
                bot_id = command.split(" ")[1]
                if bot_id in bots:
                    return bot_id
                else:
                    print(f"Бот с ID {bot_id} не найден.")
            except IndexError:
                print("Укажите ID бота для выбора.")
        elif command == "exit":
            print("Выход из программы.")
            return None
        else:
            print("Неизвестная команда. Введите 'help' для списка команд.")
