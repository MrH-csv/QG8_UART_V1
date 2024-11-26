#include <hidef.h>          /* Common defines and macros */
#include "derivative.h"     /* Include peripheral declarations */
#include <string.h>

#define BUFFER_SIZE 4
#define TEST_VALUE 0x55AA
#define TO_HEX_CHAR(n) ((n) < 10 ? '0' + (n) : 'A' + ((n) - 10)) // Macro booleano 
#define MEM_START 0x0060  // Dirección inicial segura para escribir
#define MEM_END 0x025F    // Dirección final segura para escribir (quitando los 40 del stack pointer).


void SCI_Config(void);
void Menu(void);
void UART_SendStr(const char *str);
unsigned char ReceiveByte(void);
void UART_SendHexValue(unsigned short value);
unsigned short HexCharToValue(unsigned char hex_char);

void TestMemory(void);
void ReadMemory(void);
void WriteMemory(void);
void Servo(void);
void output_control(unsigned long grados);
void PWM(unsigned int periodo, unsigned int ancho);
unsigned int decimal_conv(unsigned int hex);

unsigned int counter;  // Declaración de counter como variable global
unsigned int range;


void main(void) {
    char selector;

    SOPT1 = 0x12;  // Desactivar el watchdog timer
    SCI_Config();  // Inicializar UART

    do {
        Menu();
        // Leer la elección del usuario
        selector = ReceiveByte();//

        UART_SendStr("\n");

        switch (selector) {
            case '1':
                TestMemory();
                break;
            case '2':
                ReadMemory();
                break;
            case '3':
                WriteMemory();
                break; 
            case '4':
                 Servo();
                 break; 
            default:
                UART_SendStr("Please select one of the 4 options\n");
                break;
        }
    } while (selector != '5');
}

void SCI_Config(void) {
    SCIBDH = 0x00;  // Configurar el Baud Rate High
    SCIBDL = 0x1A;  // Configurar el Baud Rate Low para 9600 baud
    SCIC1 = 0x00;   // 8 bits de datos, sin paridad, sin loop
    SCIC2 = 0x2C;   // Receptor por IRQ, TX por Polling, Tx y RX Enable
    SCIC3 = 0x00;   // Sin Noveno Bit, ni chequeo de errores
    (void)SCIS1;    // Lectura Dummy
    SCID = 0x30;    // Para apagar banderas
}

void Menu(void) {
UART_SendStr(
		"Examen modulo SCI y PWM:\n" 
		"10 -> Validacion de memoria RAM\n"
		"20 -> Leer RAM\n"
		"30 -> Escribir en la RAM\n"
		"40 -> Ajusta el angulo del servomotor");
}

void TransmitByte(unsigned char byte) {
    SCIC2_TE = 1;   // Habilita el modulo de TX
    SCID = byte;    // Buffer de datos de TX/RX
    while (!SCIS1_TC);  // Checar bandera de TX hasta que se manda SCID
    SCIC2_TE = 0;   // Para limpiar la bandera TC
}

void UART_SendStr(const char *str) {
    while (*str) {
        TransmitByte(*str++);
    }
}

unsigned char ReceiveByte(void) {
    while (!(SCIS1 & 0x20));  // Esperar hasta que haya datos disponibles (RDRF)
    return SCID;  // Leer el carácter recibido
}

void UART_SendHexValue(unsigned short value) {
    TransmitByte(TO_HEX_CHAR((value >> 12) & 0xF));
    TransmitByte(TO_HEX_CHAR((value >> 8) & 0xF));
    TransmitByte(TO_HEX_CHAR((value >> 4) & 0xF));
    TransmitByte(TO_HEX_CHAR(value & 0xF));
}

unsigned short HexCharToValue(unsigned char hex_char) {
    if (hex_char >= '0' && hex_char <= '9') {
        return hex_char - '0';
    } else if (hex_char >= 'A' && hex_char <= 'F') {
        return hex_char - 'A' + 10;
    } else if (hex_char >= 'a' && hex_char <= 'f') {
        return hex_char - 'a' + 10;
    } else {
        return 0;
    }
}



/////////////////////////////////////////////////////   Función para probar la memoria //////////////////////////////////////////////////////////////////////
void TestMemory(void) {
    unsigned short *start_addresses[] = {(unsigned short *)0x0060, (unsigned short *)0x014B}; // Direcciones de inicio de las secciones
    unsigned short *end_addresses[] = {(unsigned short *)0x00FF, (unsigned short *)0x021F};    // Direcciones finales de las secciones
    int num_ranges = sizeof(start_addresses) / sizeof(start_addresses[0]);
    unsigned short test_value = TEST_VALUE;

    for (range = 0; range < num_ranges; range++) {
        unsigned short *current_address = start_addresses[range];
        int success = 1; // Variable para indicar si se logró escribir en todas las direcciones

        UART_SendStr("Testing memory from 0x");
        UART_SendHexValue((unsigned short)start_addresses[range]);
        UART_SendStr(" to 0x");
        UART_SendHexValue((unsigned short)end_addresses[range]);
        UART_SendStr("\n");

        // Escaneamos la memoria desde la dirección inicial hasta la final del rango actual
        while (current_address <= end_addresses[range]) {
            // Escribimos el valor 0x55AA en la dirección actual
            *current_address = test_value;
            // Leemos la dirección para verificar si la escritura fue exitosa
            if (*current_address != test_value) {
                UART_SendStr("Error writing to address 0x");
                UART_SendHexValue((unsigned short)current_address);
                UART_SendStr("\n");
                success = 0;
                break;
            }
            current_address++;
        }

        if (success) {
            UART_SendStr("OK\n");
        } else {
            UART_SendStr("Error: Memory test failed from 0x");
            UART_SendHexValue((unsigned short)start_addresses[range]);
            UART_SendStr(" to 0x");
            UART_SendHexValue((unsigned short)end_addresses[range]);
            UART_SendStr("\n");
        }
    }
}

// Función para leer una dirección de memoria especificada por el usuario
void ReadMemory(void) {
    unsigned short address = 0;
    unsigned short byte;
    unsigned char value = 0;
    unsigned char input_buffer[4] = {0}; // Buffer para almacenar la dirección ingresada
    int num_chars = 0;

    UART_SendStr("Enter the memory address to read (4 hex digits): 0x");

    // Leer hasta 4 caracteres de la dirección ingresada
    while (num_chars < 4) {
        byte = ReceiveByte();
        if (byte == '\n' || byte == '\r') {
            break; // Salir si se presiona Enter
        }
        input_buffer[num_chars++] = byte;
       // TransmitByte(byte);
    }
    UART_SendStr("\n");

    // Convertir la dirección ingresada en un valor hexadecimal
    for (counter = 0; counter < num_chars; counter++) {
        address = (address << 4) | HexCharToValue(input_buffer[counter]);
    }

    value = (*(volatile unsigned char *)address);

    UART_SendStr("The value at address 0x");
    UART_SendHexValue(address);
    UART_SendStr(" is 0x");
    UART_SendHexValue(value);
    UART_SendStr("\n");
}

// Función para escribir en una dirección de memoria especificada por el usuario
void WriteMemory(void) {
    unsigned char address = 1;
    unsigned char data = 0;
    unsigned char byte;
    unsigned char input_buffer[5] = {0}; // Buffer para almacenar la dirección ingresada
    int num_chars = 0;

    UART_SendStr("Enter the memory address to write (4 hex digits): 0x");

    // Leer hasta 4 caracteres de la dirección ingresada
    while (num_chars < 4) {
        byte = ReceiveByte();
        if (byte == '\n' || byte == '\r') {
            break; // Salir si se presiona Enter
        }
        input_buffer[num_chars++] = byte;
        TransmitByte(byte);
    }
    UART_SendStr("\n");

    // Convertir la dirección ingresada en un valor hexadecimal
    for (counter = 0; counter < num_chars; counter++) {
        address = (address << 4) | HexCharToValue(input_buffer[counter]);
    }

    // Verificar si la dirección está en el rango seguro y no es usada por el programa
    if ((address < MEM_START || address > MEM_END) && address != 0x0060 && address != 0x0100) {
        UART_SendStr("Address out of allowed range!\n");
        return;
    }
    
    UART_SendStr("Enter the data to write (4 hex digits): 0x");
    num_chars = 0; // Reiniciar el contador de caracteres

    // Leer hasta 4 caracteres del dato ingresado
    while (num_chars < 4) {
        byte = ReceiveByte();
        if (byte == '\n' || byte == '\r') {
            break; // Salir si se presiona Enter
        }
        input_buffer[num_chars++] = byte;
        TransmitByte(byte);
    }
    UART_SendStr("\n");

    // Convertir el dato ingresado en un valor hexadecimal
    for (counter = 0; counter < num_chars; counter++) {
        data = (data << 4) | HexCharToValue(input_buffer[counter]);
    }

    *(volatile unsigned char *)address = data;

    UART_SendStr("Data 0x");
    UART_SendHexValue(data);
    UART_SendStr(" written to address 0x");
    UART_SendHexValue(address);
    UART_SendStr("\n");
}

void Servo(void){
	unsigned int num_grados = 0;
    unsigned int byte;
    unsigned char value = 0;
    unsigned char input_buffer[4] = {0}; // Buffer para almacenar la dirección ingresada
    int num_chars = 0;
    unsigned int decimal = 0;

    UART_SendStr("Ingresa los grados a los que quieres establecer el servomotor:");

    // Leer hasta 4 caracteres de la dirección ingresada
    while (num_chars < 4){
        byte = ReceiveByte();
        if (byte == '\n' || byte == '\r') {
            break; // Salir si se presiona Enter
        }
        input_buffer[num_chars++] = byte;
       // TransmitByte(byte);
    }
    UART_SendStr("\n");
    
    // Convertir la dirección ingresada en un valor hexadecimal
       for (counter = 0; counter < num_chars; counter++) {
    	   num_grados = (num_grados << 4) | HexCharToValue(input_buffer[counter]);
       }
       
       decimal = decimal_conv(num_grados);  
       
       output_control(decimal);
       UART_SendStr("El angulo del servomotor es:");
       UART_SendHexValue(num_grados);
       UART_SendStr("º");
       UART_SendStr("\n");
}

void output_control(unsigned long grados){//esta funcion contiene el main del codigo para PWM y ADC
	unsigned int ADCresult = 0;
	unsigned int ADCresult1 = 0;
	unsigned long conv_cuentas = 0;

	grados = grados * 6944; // 0.694;
	conv_cuentas = grados / 10000 ;//conversion por punto fijo.
	ADCresult = 125;
	ADCresult = ADCresult + conv_cuentas;
	
	if(ADCresult >= 249){
		ADCresult = 320;
	}
	if(ADCresult <= 125){
			ADCresult = 90;
		}
	
	if(ADCresult1<ADCresult-3 ||ADCresult1>ADCresult+3){
		ADCresult1=ADCresult;
		TPMCNT = 0X0000;
		TPMSC=0X00;
	}
	PWM(2500,ADCresult1);//2500 CUENTAS = 20ms = 50hz	
}

void PWM(unsigned int periodo, unsigned int ancho){
TPMMOD=periodo;//time of the cycle (valued in quantity of counts). 
TPMC1V=ancho + 1;//High time + offset.
TPMSC = 0x0D; // Sin IRQ, BusClk=4Mhz, Preescaler=32
TPMC1SC = 0x28; //ModoPWM, Aligned rising edge.
while(!TPMC1SC_CH1F);//Wait for the flag (End of High time).
TPMC1SC_CH1F=0; // Clear the flag to CH1 (TPM enabled as PWM).
}

unsigned int decimal_conv(unsigned int hex){
	unsigned int temp0 =0;
	unsigned int modu0 = 0;
	unsigned int temp1 =0;
	unsigned int modu1 = 0;
	unsigned int temp2 =0;
	unsigned int modu2 = 0;
	unsigned int res0 = 0;
	unsigned int res1 = 0;
	unsigned int res2 = 0;
	unsigned int unidades = 0;
	unsigned int decenas = 0;
	unsigned int centenas = 0;
	unsigned int v_decimal=0;
	
	temp0 = hex /16;//24
	modu0 = temp0*16;
	res0 = hex - modu0;//24 - 24 = 0 
	
	temp1 = temp0 /16;//1.5
	modu1 = temp1*16; //1*16
	res1 = temp0 - modu1;//24 - 16 = 8
	
	temp2=temp1;// 1
	res2=temp2;//1
	
	unidades = res0;
	
	decenas = res1 * 10;
	
	centenas = res2 * 100;
	
	v_decimal = centenas + decenas + unidades;
	
	
	return v_decimal;
}
