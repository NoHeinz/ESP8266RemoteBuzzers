from serial import Serial
from serial.threaded import ReaderThread, FramedPacket
from findport import serial_ports
from enum import Enum
import queue

class Button_State(Enum):
        Disabled = 1
        Enabled = 2
        Answered = 3
        
class qSerialReaderProtocolRaw(FramedPacket):
    port = None
    START = b'S'
    STOP = b'\n'

    def __init__(self, q) -> None:
        super().__init__()
        self.q = q
    
    def handle_packet(self, packet):
        if len(packet) == 4:
            self.q.put(packet)

class qReaderThread(ReaderThread):
    def __init__(self, serial_instance, protocol_factory, q):
        """\
        Initialize thread.

        Note that the serial_instance' timeout is set to one second!
        Other settings are not changed.
        """
        super(qReaderThread, self).__init__(serial_instance, protocol_factory)
        self.q = q
        
    def run(self):
        """Reader loop"""
        if not hasattr(self.serial, 'cancel_read'):
            self.serial.timeout = 1
        self.protocol = self.protocol_factory(self.q)
        try:
            self.protocol.connection_made(self)
        except Exception as e:
            self.alive = False
            self.protocol.connection_lost(e)
            self._connection_made.set()
            return
        error = None
        self._connection_made.set()
        while self.alive and self.serial.is_open:
            try:
                # read all that is there or wait for one byte (blocking)
                data = self.serial.read(self.serial.in_waiting or 1)
            except serial.SerialException as e:
                # probably some I/O problem such as disconnected USB serial
                # adapters -> exit
                error = e
                break
            else:
                if data:
                    # make a separated try-except for called user code
                    try:
                        self.protocol.data_received(data)
                    except Exception as e:
                        error = e
                        break
        self.alive = False
        self.protocol.connection_lost(error)
        self.protocol = None
        

class serial_to_controller():
    packet_q = queue.Queue()
    port = ""
    connected_buttons = []
    button_states = []
    has_answered = []

    def __init__(self, port = None):
        if port == None:
            port = serial_ports()[0]
        self.port = port
        self.serial_port = Serial(self.port, baudrate = 57600)    
        self.reader = qReaderThread(self.serial_port, qSerialReaderProtocolRaw, self.packet_q)
        self.reader.start()
        self.connected_buttons = [True, True, True, True]
        self.button_states = [Button_State.Enabled, Button_State.Enabled, Button_State.Enabled, Button_State.Enabled]
        self.has_answered = [True, True, True, True]
        
    def update_data(self):
        while(True):
            try:
                temp = self.packet_q.get_nowait()
                recentdata = temp
            except queue.Empty:
                if 'recentdata' in locals():
                    for  i, byte in enumerate(recentdata):
                        self.connected_buttons[i] = True if (byte & 0x80 != 0) else False
                        self.has_answered[i] = True if (byte & 0x40 != 0) else False
                        self.button_states[i] = Button_State(byte & 0x0F)
                    break
                else:
                    break
            
    def print_data(self):
        for  i in range(len(self.connected_buttons)):
            print(f"button{i} State:{self.button_states[i]}, Answered:{self.has_answered[i]}, connected:{self.connected_buttons[i]}")

    def ready_buttons(self):
        self.reader.write(bytearray([1]))
    
    def reset_buttons(self):
        self.reader.write(bytearray([2]))
    
    def disable_buttons(self):
        self.reader.write(bytearray([3]))
        
    def set_answered_status(self, button, status):
        self.reader.write(bytearray([4, button, status]))

h = serial_to_controller()
while True:
    imp = int(input("enter choice"))
    if imp == 1:
        h.ready_buttons()
    elif imp == 2:
        h.reset_buttons()
    elif imp == 3:
        h.disable_buttons()
    elif imp == 4:
        h.set_answered_status(int(input("button")), int(input("status")))
    elif imp == 5:
        h.update_data()
        h.print_data()



    

