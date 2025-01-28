import random
import string
import time
import socket
import struct

def generate_random_code(length=4):
    """
    Generate a random string of letters of specified length.
    """
    return ''.join(random.choices(string.ascii_lowercase, k=length))

def send_udp_command(ip, port, message):
    """
    Send a message as a UDP packet to the specified IP and port.
    """
    # Calculate the length of the message (in bytes)
    message_length = len(message)
    
    # Create the 4-byte size prefix in network byte order (big-endian)
    size_prefix = struct.pack('!I', message_length)
    
    # Combine the size prefix and the message
    udp_packet = size_prefix + message.encode('utf-8')
    
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    # Send the packet to the specified IP and port
    sock.sendto(udp_packet, (ip, port))
    
    # Close the socket
    sock.close()

def request_keyframe(ip, port, num_attempts=5, message_interval=0.025):
    """
    Generate a unique message requesting a keyframe and send it as a UDP packet multiple times.
    """
    # Generate a random code for this request
    unique_code = generate_random_code()

    # Create the special message with the unique code
    special_message = f"special:request_keyframe:{unique_code}"

    for attempt in range(num_attempts):
        print(f"Sent special message: {special_message}, attempt {attempt + 1}/{num_attempts}")
        send_udp_command(ip, port, special_message)
        time.sleep(message_interval)  # Wait before the next attempt

if __name__ == "__main__":
    # Define the target IP and port
    target_ip = "10.5.0.10"  # Target IP
    target_port = 9999      # Target Port

    try:
        request_keyframe(target_ip, target_port)
        #time.sleep(2)
        #request_keyframe(target_ip, target_port)
    except KeyboardInterrupt:
        print("\nExecution stopped by user.")
