import argparse
import os
import re
import socket
import threading
import time

def parse_adapters(file_path):
    """Parses the adapter names from the specified file."""
    with open(file_path, 'r') as f:
        content = f.read()
    # Match everything between double quotes and split by whitespace
    adapters = re.findall(r'"([^"]*)"', content)
    if adapters:
        return adapters[0].strip().split()  # Return the first match
    return []

def update_ports(adapter_names, first_port):
    """Generates output ports based on adapter names and the first port."""
    return [first_port + i for i in range(len(adapter_names))]

def forward_message(udp_socket, message, output_ports):
    """Sends the received message to the current output port in round-robin fashion."""
    global round_robin_index
    if output_ports:
        current_port = output_ports[round_robin_index]
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as out_socket:
            out_socket.sendto(message, ('localhost', current_port))  # Forward to the output port
        round_robin_index = (round_robin_index + 1) % len(output_ports)

def listen_for_messages(port_in, output_ports):
    """Listens for incoming UDP messages and forwards them."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
        udp_socket.bind(('localhost', port_in))
        while True:
            message, _ = udp_socket.recvfrom(1024)  # Buffer size is 1024 bytes
            forward_message(udp_socket, message, output_ports)

def monitor_adapters(file_path, first_port, interval):
    """Monitors the adapter file for changes and updates the output ports."""
    global output_ports
    global last_mod_time
    while True:
        current_mod_time = os.path.getmtime(file_path)
        if current_mod_time != last_mod_time:
            print("Detected change in adapters file. Updating ports...")
            adapter_names = parse_adapters(file_path)
            output_ports = update_ports(adapter_names, first_port)
            print(f"Updated output ports: {output_ports}")
            last_mod_time = current_mod_time
        time.sleep(interval)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="UDP Message Forwarder")

    parser.add_argument('port_in', type=int, help='Port to listen for incoming messages')
    parser.add_argument('first_port_out', type=int, help='First port to use for output')

    args = parser.parse_args()

    # Global variables
    round_robin_index = 0
    output_ports = []
    last_mod_time = 0

    # Initialize adapter ports
    adapter_names = parse_adapters('/etc/default/wifibroadcast')
    output_ports = update_ports(adapter_names, args.first_port_out)
    print(f"Initial output ports: {output_ports}")

    # Start threads
    threading.Thread(target=listen_for_messages, args=(args.port_in, output_ports), daemon=True).start()
    threading.Thread(target=monitor_adapters, args=('/etc/default/wifibroadcast', args.first_port_out, 10), daemon=True).start()

    # Keep the main thread alive
    while True:
        time.sleep(1)
