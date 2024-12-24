# NODEPP-Bluetooth
Run **Bluetooth** in Nodepp

## Dependencies
```bash
# Bluez ( Linux Only )
  🐧: sudo apt install libbluetooth-dev

# Nodepp
  💻: https://github.com/NodeppOficial/nodepp
```

## Build & Run
```bash
🪟: g++ -o main main.cpp -I ./include -lws2_32    ; ./main
🐧: g++ -o main main.cpp -I ./include -lbluetooth ; ./main
```
