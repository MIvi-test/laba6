from string import ascii_letters,digits
from random import randint,choice

alf = ascii_letters + digits

def generate_random_string(length=15):
    return ''.join(choice(alf) for _ in range(length))

print(generate_random_string())
with open("big_file.txt", "a", encoding="utf-8") as file:
    max_int = 2**31-1
    for i in range(5_000_000):
        file.write(f"put {generate_random_string()} = {randint(0,max_int)}\n")
    file.write(f"put basfd\n")
    
        

    