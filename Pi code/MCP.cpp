#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdint.h>
#include "packet.h"
#include "serial.h"
#include "serialize.h"
#include "constants.h"

#include "make_tls_server.h"
#include "tls_common_lib.h"

#include <stdio.h>

#define PORTNUM 5000
#define KEY_FNAME   "alex.key"
#define CERT_FNAME  "alex.crt"
#define CA_CERT_FNAME   "signing.pem"
#define CLIENT_NAME     "laptop.epp.com"

#define PORT_NAME			"/dev/ttyACM0"
#define BAUD_RATE			B9600 //Look at this again

static char buffer[128];

int exitFlag=0;
sem_t _xmitSema;

void handleError(TResult error)
{
	switch(error)
	{
		case PACKET_BAD:
			printf("ERROR: Bad Magic Number\n");
			break;

		case PACKET_CHECKSUM_BAD:
			printf("ERROR: Bad checksum\n");
			break;

		default:
			printf("ERROR: UNKNOWN ERROR\n");
	}
}

void handleStatus(TPacket *packet)
{
	printf("\n ------- ALEX STATUS REPORT ------- \n\n");
	printf("Left Forward Ticks:\t\t%d\n", packet->params[0]);
	printf("Right Forward Ticks:\t\t%d\n", packet->params[1]);
	printf("Left Reverse Ticks:\t\t%d\n", packet->params[2]);
	printf("Right Reverse Ticks:\t\t%d\n", packet->params[3]);
	printf("Left Forward Ticks Turns:\t%d\n", packet->params[4]);
	printf("Right Forward Ticks Turns:\t%d\n", packet->params[5]);
	printf("Left Reverse Ticks Turns:\t%d\n", packet->params[6]);
	printf("Right Reverse Ticks Turns:\t%d\n", packet->params[7]);
	printf("Forward Distance:\t\t%d\n", packet->params[8]);
	printf("Reverse Distance:\t\t%d\n", packet->params[9]);
	printf("\n---------------------------------------\n\n");
}

void handleResponse(TPacket *packet)
{
	// The response code is stored in command
	switch(packet->command)
	{
		case RESP_OK:
			printf("Command OK\n");
		break;

		case RESP_STATUS:
			handleStatus(packet);
		break;

		default:
			printf("Arduino is confused\n");
	}
}

void handleErrorResponse(TPacket *packet)
{
	// The error code is returned in command
	switch(packet->command)
	{
		case RESP_BAD_PACKET:
			printf("Arduino received bad magic number\n");
		break;

		case RESP_BAD_CHECKSUM:
			printf("Arduino received bad checksum\n");
		break;

		case RESP_BAD_COMMAND:
			printf("Arduino received bad command\n");
		break;

		case RESP_BAD_RESPONSE:
			printf("Arduino received unexpected response\n");
		break;

		default:
			printf("Arduino reports a weird error\n");
	}
}

void handleMessage(TPacket *packet)
{
	printf("Message from Alex: %s\n", packet->data);
}

void handlePacket(TPacket *packet)
{
	switch(packet->packetType)
	{
		case PACKET_TYPE_COMMAND:
				// Only we send command packets, so ignore
			break;

		case PACKET_TYPE_RESPONSE:
				handleResponse(packet);
			break;

		case PACKET_TYPE_ERROR:
				handleErrorResponse(packet);
			break;

		case PACKET_TYPE_MESSAGE:
				handleMessage(packet);
			break;
	}
}

void sendPacket(TPacket *packet)
{
	char buffer[PACKET_SIZE];
	int len = serialize(buffer, packet, sizeof(TPacket));
	printf("INSIDE SEND PACKET FUNCTION\n");
	serialWrite(buffer, len);
}

void *receiveThread(void *p)
{
	char buffer[PACKET_SIZE];
	int len;
	TPacket packet;
	TResult result;
	int counter=0;

	while(1)
	{
		len = serialRead(buffer);
		counter+=len;
		if(len > 0)
		{
			result = deserialize(buffer, len, &packet);

			if(result == PACKET_OK)
			{
				counter=0;
				handlePacket(&packet);
			}
			else 
				if(result != PACKET_INCOMPLETE)
				{
					printf("PACKET ERROR\n");
					handleError(result);
				}
		}
	}
}

void flushInput()
{
	char c;

	while((c = getchar()) != '\n' && c != EOF);
}

void getParams(TPacket *commandPacket)
{
	//printf("Enter distance/angle in cm/degrees (e.g. 50) and power in %% (e.g. 75) separated by space.\n");
	//printf("E.g. 50 75 means go at 50 cm at 75%% power for forward/backward, or 50 degrees left or right turn at 75%%  power\n");
	commandPacket->params[0] = ((int32_t)buffer[2] - 48)*10 + ((int32_t)buffer[3] - 48);
	commandPacket->params[1] = ((int32_t)buffer[5] - 48)*10 + ((int32_t)buffer[6] - 48);
	//scanf("%d %d", &commandPacket->params[0], &commandPacket->params[1]);
	printf("%u %u \n", commandPacket->params[0], commandPacket->params[1]);
	printf("%s\n", buffer);
	printf("%c %u\n", buffer[2], (int32_t)buffer[2]);
}

void getClearCommandParams(TPacket *commandPacket)
{
	printf("which one u want to clear?.\n");
	scanf("%d", &commandPacket->params[0]);
	flushInput();
}

void sendCommand(char command)
{
	TPacket commandPacket;

	commandPacket.packetType = PACKET_TYPE_COMMAND;

	switch(command)
	{
		case 'f':
		case 'F':
			getParams(&commandPacket);
			commandPacket.command = COMMAND_FORWARD;
			sendPacket(&commandPacket);
			break;

		case 'b':
		case 'B':
			getParams(&commandPacket);
			commandPacket.command = COMMAND_REVERSE;
			sendPacket(&commandPacket);
			break;

		case 'l':
		case 'L':
			getParams(&commandPacket);
			commandPacket.command = COMMAND_TURN_LEFT;
			sendPacket(&commandPacket);
			break;

		case 'r':
		case 'R':
			getParams(&commandPacket);
			commandPacket.command = COMMAND_TURN_RIGHT;
			sendPacket(&commandPacket);
			break;

		case 's':
		case 'S':
			commandPacket.command = COMMAND_STOP;
			sendPacket(&commandPacket);
			break;

		case 'c':
		case 'C':
			getClearCommandParams(&commandPacket);
			commandPacket.command = COMMAND_CLEAR_STATS;
			sendPacket(&commandPacket);
			break;

		case 'g':
		case 'G':
			commandPacket.command = COMMAND_GET_STATS;
			sendPacket(&commandPacket);
			break;

		case 'q':
		case 'Q':
			exitFlag=1;
			break;

		default:
			printf("Bad command\n");

	}
}

void clearBuffer() {
	for (long i = 0; i < 128; i += 1) {
		buffer[i] = 0;
	}
}

void *worker(void *conn) {
    int exit = 0;

    while(!exit) {
		
        int count;
        //char buffer[128];
		clearBuffer();
        count = sslRead(conn, buffer, sizeof(buffer));

        if(count > 0) {
            //printf("Read %s. Echoing.\n", buffer);
            //count = sslWrite(conn, buffer, sizeof(buffer));
            sendCommand(buffer[0]);
            printf("INSIDE WHILE LOOP\n");
			// flushInput();
            

            if(count < 0) {
                perror("Error writing to network: " );
            }
        }
        else if(count < 0) {
            perror("Error reading from network: ");
        }
    
        // Exit of we have an error or the connection has closed.
        exit = (count <= 0);
    }

    printf("\nConnection closed. Exiting.\n\n");
    EXIT_THREAD(conn);
}

int main()
{
	// Connect to the Arduino
	startSerial(PORT_NAME, BAUD_RATE, 8, 'N', 1, 5);

	// Sleep for two seconds
	printf("WAITING TWO SECONDS FOR ARDUINO TO REBOOT\n");
	sleep(2);
	printf("DONE\n");

	// Spawn receiver thread
	pthread_t recv;

	pthread_create(&recv, NULL, receiveThread, NULL);

	// Send a hello packet
	TPacket helloPacket;

	helloPacket.packetType = PACKET_TYPE_HELLO;
	sendPacket(&helloPacket);

	//TLS communication between laptop and Raspberry Pi
	createServer(KEY_FNAME, CERT_FNAME, PORTNUM, &worker, CA_CERT_FNAME, CLIENT_NAME, 1);
	
	while(server_is_running());
/**
	while(!exitFlag)
	{
		char ch;
		printf("Command (f=forward, b=reverse, l=turn left, r=turn right, s=stop, c=clear stats, g=get stats q=exit)\n");
		//scanf("%c", &ch);

		// Purge extraneous characters from input stream
		flushInput();

		sendCommand(ch);
	}
	**/

	printf("Closing connection to Arduino.\n");
	endSerial();
}


/**
Change log:
Why are the values not being read? different type sizes for pi and arduino? or other code issue?
After it's read once, how to make it continously read the values again? (read x_y_z)
changed line 183 and 184
purpose of function at line 134, sendpacket? which is the one sending to arduino?
Find our how to use the readssl function, so that it reads all 3 values into the buffer. what does the space do? then change it to buffer[1] and buffer[2] in getparams
Implement the readssl function in the getparams fn?
used int32_t instead of int for the geetparams fn
**/
