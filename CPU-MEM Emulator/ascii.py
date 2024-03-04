STR = " _   _      _ _         _    _            _     _ !| | | |    | | |       | |  | |          | |   | |!| |_| | ___| | | ___   | |  | | ___  _ __| | __| |!|  _  |/ _ \ | |/ _ \  | |/\| |/ _ \| '__| |/ _` |!| | | |  __/ | | (_) | \  /\  / (_) | |  | | (_| |!\_| |_/\___|_|_|\___/   \/  \/ \___/|_|  |_|\__,_|!"

if __name__ == "__main__":
    last_c = ""
    count = 0
    for c in STR:
        if c != last_c and count > 0:
            if last_c == "!":
                print(ord("\n"))
            else:
                print(ord(last_c))
            print(count)
            count = 0
        last_c = c
        count += 1
    if last_c == "!":
        print(ord("\n"))
    else:
        print(ord(last_c))
    print(count)
    print(0)
