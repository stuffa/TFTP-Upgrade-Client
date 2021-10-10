
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>


#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#ifdef WIN32
	#include <winsock.h>
#else
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
#endif



/*
 * Trivial File Transfer Protocol (IEN-133)
 */

// Version now matches the semantic version numbering as recommended by github
#define VERSION					"1.0.3"

#define	SEGSIZE	512 												// Data payload size
#define PKTSIZE 				SEGSIZE+sizeof(struct tftp_header)  // Max packet size
#define TFTP_PSYDO_FILENAME		"upgrade"							// The name given to the file we upload
#define TFTP_XFER_MODE			"octal"								// The file is binary so the mode MUST be octal

/*
 * TFTP Opcodes - Packet types.
 */
enum tftp_opcodes
{
	TFTP_RRQ   = 1,			/* Read request     */
	TFTP_WRQ   = 2,			/* Write request    */
	TFTP_DATA  = 3,		    /* Data packet      */
	TFTP_ACK   = 4,			/* Acknowledgment   */
	TFTP_ERROR = 5, 		/* Error code       */
};

struct tftp_data
{
	unsigned short	block_number;		/* block # */
	char 			data[];
} __attribute__((packed));

struct tftp_ack
{
	unsigned short	block_number;
} __attribute__((packed));

struct tftp_error
{
	unsigned short	err_code;
	char			err_msg[];
} __attribute__((packed));

struct	tftp_header
{
	unsigned short	opcode;					/* packet type */
	union
	{
		struct tftp_data	data;
		struct tftp_ack		ack;
		struct tftp_error	error;
		char				request[2];
	};
} __attribute__((packed));

/*
 * Error codes.
 */
enum tftp_error_codes
{
	EUNDEF    = 0,		/* not defined */
	ENOTFOUND = 1,		/* file not found */
	EACCESS	  = 2,		/* access violation */
	ENOSPACE  = 3,		/* disk full or allocation exceeded */
	EBADOP	  = 4,		/* illegal TFTP operation */
	EBADID	  = 5,		/* unknown transfer ID */
	EEXISTS	  = 6,		/* file already exists */
	ENOUSER	  = 7,		/* no such user */
};


void display_error_code(struct tftp_error *e)
{
	fprintf(stderr, " Failed\n"
					"ERROR: TFTP server error code 0x%hx : %s\n", ntohs(e->err_code), e->err_msg);
	fprintf(stderr, "\n"
					"Error Codes:\n"
					"    0 - Not defined\n"
					"    1 - File not found\n"
					"    2 - Access violation\n"
					"    3 - Disk full or allocation exceeded\n"
			        "    4 - Illegal TFTP operation\n"
					"    5 - Unknown transfer Id\n"
					"    6 - File already exists\n"
					"    7 - No such user\n"
					"\n" );
}


int tftp_send(struct sockaddr_in *destination, int file)
{
	// Allocate the sending buffer
    char xmit_buf[PKTSIZE];
    // Allocate the receive buffer
    char receive_buf[PKTSIZE];

    struct tftp_header *xmit    = (struct tftp_header *)xmit_buf;
    struct tftp_header *receive = (struct tftp_header *)receive_buf;


	// Create the socket
	int		tftp_socket;
	if ( (tftp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 )
	{
		perror("\nERROR: socket(): Unable to create UDP socket");
		return -1;
	};

	// make up a write request
	xmit->opcode = htons((u_short)TFTP_WRQ);
	char *request_ptr = xmit->request;

	// Add the file name to the request
	char *tftp_filename = TFTP_PSYDO_FILENAME;
	strcpy(request_ptr, tftp_filename);
	request_ptr += strlen(tftp_filename) +1 ;		// include the NULL

	// Add the transfer mode to the request
	char *tftp_mode = TFTP_XFER_MODE;
	strcpy(request_ptr, tftp_mode);
	request_ptr += strlen(tftp_mode) +1;	// include the NULL
	int size = request_ptr - xmit_buf;

    struct timeval	receive_timeout;
	receive_timeout.tv_sec  = 0;
	receive_timeout.tv_usec = 500000;		// 0.5 seconds

	time_t retry_timeout  = time(NULL) + 30;	// retry for 30 seconds

	int retry_count = 0;			// used to display the reboot router message after a number of attempts
	int	time_out	= 1;			// indicates if we timed out - assume we did and clear when we receive a packet

	while ( time(NULL) <= retry_timeout )
	{
		// Allow 4 retries before deciding that the router may not be turned on
		if ( retry_count++ == 4 )	// second timeout, without sending anything
		{
			fprintf( stdout,
					"\nREBOOT the router NOW.. The upgrade will then start\n"
					"Waiting 30 seconds for the router to be rebooted\n"
					"\n");
		}

		// send the write request
		int byte_count = sendto(tftp_socket, xmit_buf, size, 0, (struct sockaddr *)destination, sizeof(struct sockaddr_in));
		if (byte_count != size)
		{
    		if ( byte_count < 0 )
    			perror("\nERROR: Sendto()");
    		else
    			fprintf(stderr, "\nError: Only sent %d bytes of %d\n", byte_count, size );

   			close(tftp_socket);
   			return -1;
   		};

		int	file_count;
		fd_set	file_set;
		FD_ZERO(&file_set);
		FD_SET(tftp_socket, &file_set);

		receive_timeout.tv_sec  = 0;
		receive_timeout.tv_usec = 500000;		// 0.5 seconds

		if ( (file_count = select(tftp_socket+1, &file_set, NULL, NULL, &receive_timeout)) < 0 )
		{
			perror("\nERROR: select()");
			close(tftp_socket);
			return -1;
		};

		if (file_count == 0 ) // time out
			continue;

		// read what is in the buffer
		struct sockaddr_in	from;
		int	from_size = sizeof(from);

		if ( (byte_count = recvfrom(tftp_socket, (char *)receive, PKTSIZE, 0, (struct sockaddr *)&from, &from_size)) < 0 )
			// We may get a read failure if the packet was an ICMP message
			// so just ignore it
			continue;

		// check length
		if ( byte_count < sizeof(struct tftp_header) )
			continue;

		// check it came from the correct IP address
		if ( (destination->sin_family != from.sin_family) || (destination->sin_addr.s_addr != from.sin_addr.s_addr) )
			continue;

		// Was it an ACK or ERROR
		switch ( ntohs(receive->opcode) )
		{
		case TFTP_ACK:
			if ( ntohs(receive->ack.block_number) == 0 )
			{
				time_out = 0;
				fprintf(stdout, "Sending:");
				fflush(stdout);
				// use the port that the server responded from
				destination->sin_port = from.sin_port;
				// Connect so that we only receive data from this port in the future
				connect(tftp_socket, (struct sockaddr *)destination, sizeof(struct sockaddr_in));
			}
			else
				continue;
			break;

		case TFTP_ERROR:
			display_error_code(&(receive->error));
			close(tftp_socket);
			return -1;
			break;

		default:
			// corrupted packet - retry
			continue;
			break;
		}

		break;
	}

	// check if we timed out
	if (time_out)
	{
		fprintf(stderr, "\nERROR: Timeout\n");
	    close(tftp_socket);
		return -1;
	}

	// we have started the transfer
	// the socket has been connected to the port used for the first reply

	receive_timeout.tv_sec  = 1;		// 1 second
	receive_timeout.tv_usec = 0;		//

	retry_timeout  = time(NULL) + 10;		// retry for 10 seconds

	// get the file size
	struct stat file_stat;
	if ( fstat(file, &file_stat) < 0 )
	{
		perror(" Failed\nERROR: fstat()");
		close(tftp_socket);
		return -1;
	}

	int blocks_per_dot	= file_stat.st_size/(SEGSIZE*60);
	int dot_count		= 0;
	int retry			= 0;
	int block_number	= 0;
	int last_packet		= 0;

	time_out			= 1;

	// make sure that we read from the front of the file
	lseek(file, 0, SEEK_SET);

	while (time(NULL) <= retry_timeout)
	{
		if ( ! retry )
		{
			retry_timeout = time(NULL) + 10;
			if ( ++dot_count == blocks_per_dot )
			{
				dot_count = 0;
				fprintf(stdout, ".");
				fflush(stdout);
			}
			// populate the next packet to send
			block_number++;
			xmit->opcode = htons(TFTP_DATA);
			xmit->data.block_number = htons(block_number);

			size = read(file, xmit->data.data, SEGSIZE);

			if (size < 0)
			{
				perror(" \nERROR: Reading file");
				close(tftp_socket);
				return -1;
			}

			if ( size != SEGSIZE )
				last_packet = 1;

			size += sizeof(struct tftp_header);
			retry = 1;
		}

		// send the data block
		int byte_count = send(tftp_socket, xmit_buf, size, 0);
		if (byte_count != size)
		{
    		if ( byte_count < 0 )
    			perror(" \nERROR: Send()");
    		else
    			fprintf(stderr, " \nERROR: Only sent %d bytes of %d\n", byte_count, size );

   			close(tftp_socket);
   			return -1;
   		};

		int	file_count;
		fd_set	file_set;
		FD_ZERO(&file_set);
		FD_SET(tftp_socket, &file_set);

		receive_timeout.tv_sec  = 1;		// 1 second
		receive_timeout.tv_usec = 0;		//

		if ( (file_count = select(tftp_socket+1, &file_set, NULL, NULL, &receive_timeout)) < 0 )
		{
			perror("\nERROR: select()");
			close(tftp_socket);
			return -1;
		};

		if (file_count == 0 ) // time out
			// retry
			continue;

		if ( (byte_count = recv(tftp_socket, (char *)receive, PKTSIZE, 0)) < 0 )
			// probably ICMP
			continue;

		// check length
		if ( byte_count < sizeof(struct tftp_header) )
			continue;

		// Was it an ACK or ERROR
		switch ( ntohs(receive->opcode) )
		{
		case TFTP_ACK:
			if ( ntohs(receive->ack.block_number) == block_number )
				retry = 0;
			else
				continue;
			break;

		case TFTP_ERROR:
			display_error_code(&(receive->error));
			close(tftp_socket);
			return -1;
			break;

		default:
			// corrupted packet - retry
			continue;
			break;
		}

		if (last_packet)
		{
			time_out = 0;
			fprintf(stdout, " OK\n");
			fflush(stdout);
			break;
		}
	}

	// finished
    close(tftp_socket);

	// check for timeout
	if (time_out )
	{
		fprintf(stderr, "\nERROR: Timeout\n");
	    return -1;
	}

    return 0;
};

void set_arp_entry(unsigned int mac[6], uint32_t ip_addr)
{
	return;

//	struct arpreq ar;
//	struct hostent *hp;
//	struct sockaddr_in *sin;
//	u_char *ea;
//	int s;
//
//	bzero((caddr_t)&ar, sizeof ar);
//	sin = (struct sockaddr_in *)&ar.arp_pa;
//	sin->sin_family = AF_INET;
//
//	sin->sin_addr.s_addr = ip_addr;
//
//	ea = (u_char *)ar.arp_ha.sa_data;
//
//	if (ether_aton(mac, ea))
//		return (1);
//
//	ar.arp_flags = ATF_PERM;
//
//	s = socket(AF_INET, SOCK_DGRAM, 0);
//	if (s < 0)
//	{
//		perror("arp: socket");
//		exit(-1);
//	}
//	if (ioctl(s, SIOCSARP, (caddr_t)&ar) < 0)
//	{
//		perror(host);
//		exit(-1);
//	}
//	close(s);
//	return (0);
}

void remove_arp_entry(unsigned int mac[8])
{
	return;
}


void version(void)
{
	fprintf(stderr, "\nTFTP Upgrade Client Version %s\n", VERSION);
}


void usage(void)
{
//	version();  already done
	fprintf(stderr, "\n"
					"usage: upgrade [options] <destination> <file_name>\n"
					"\n"
					"<destination>  : FQDN or IP address of destination.\n"
					"<file_name>    : name of the file to send.\n"
					"-h             : display this message\n"
					"-v             : display the version\n"
					"-p port_number : use a different port.  Default = UDP port 69\n"
					"-a <mac_addr>  : specify the mac address to get a faster connection\n"
					"                 (not implemented yet)\n"
					"\n"
					"All transfers are binary.\n"
					"Retry 2 times every second.\n"
					"Timeout after 30 seconds.\n"
					"\n");
}


int main(int argc, char **argv)
{
    int 			opt;
    int				tftp_port	= 69;
    unsigned int	mac[6];
//   struct	 arpreq arp_ioctl;
    int				set_arp		= 0;

    // announce who we are
    version();


    while ( (opt = getopt(argc, argv, "hva:p:")) !=  EOF)
    {
    	switch (opt)
    	{
    	case 'a':	// aggressive
    		if ( sscanf(optarg, "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6 )
    		{
    			fprintf(stderr, "\nERROR: unable to mac address passed with \"-a\" argument.\n");
    			exit(3);
    		}
//    		memset(arp_ioctl, 0, sizeof(arp_ioctl));
    		set_arp = 1;
    		break;

    	case 'h':
    		usage();
    		exit(0);
    		break;

    	case 'p':
    		tftp_port = atoi(optarg);
    	    if ( tftp_port == 0 )
    	    {
    	        fprintf(stderr,"\nERROR: Invalid port number.\n");
    	        exit(3);
    	    }

    		break;

    	case 'v':
    		// version();	its done above
    		exit(0);
    		break;

    	default:
    		fprintf(stderr, "\nERROR: Invalid Option\n");
    		usage();
    		exit(-1);
    	}
    }

    // There should be 2 more arguments
    if ( (argc - optind) != 2 )
    {
    	fprintf(stderr, "\nERROR: Insufficient arguments\n");
    	usage();
    	exit(2);
    }

    char *destination_arg	= argv[optind++];
    char *filename_arg		= argv[optind];

#ifdef WIN32
    // Prepare windows socket layer
    unsigned short	wsa_version = (1<<8) | 1;
    struct WSAData	wsa_data;

    WSAStartup(wsa_version, &wsa_data);
#endif	// WIN32

    /* Is the host name resolvable */
    int ret_val = 0;
    struct hostent 		*hp;
    struct sockaddr_in	destination_address;
    if ( (hp = gethostbyname(destination_arg)) == NULL )
    {
        fprintf(stderr, "\nERROR: destination %s is not resolvable.\n", destination_arg);
        ret_val = 2;
    }
    else
    {
    	// populate the destination address
    	memset(&destination_address, 0, sizeof(struct sockaddr_in));
		destination_address.sin_family=AF_INET;
		destination_address.sin_port=htons(tftp_port);
		memcpy(&destination_address.sin_addr.s_addr, hp->h_addr, hp->h_length);

		// set the MAC address in the ARP table if we need to
		if (set_arp)
			set_arp_entry(mac, destination_address.sin_addr.s_addr);

	    int file;

#ifdef WIN32
		if ( (file = open(filename_arg, (O_RDONLY | O_BINARY))) < 0 )
#else
		if ( (file = open(filename_arg, (O_RDONLY))) < 0 )
#endif
		{
			perror("\nERROR: unable to open file");
			ret_val = -1;
		}
		else
		{
			ret_val = tftp_send(&destination_address, file);
			close(file);

			if ( ret_val != 0 )
				fprintf(stderr, "\nERROR: Upgrade failed\n\n");
			else
			{
				fprintf( stdout,
						"\n"
						"Allow a couple of minutes for the update to be flashed to the device\n"
						"\n" );
				fflush(stdout);
			}
		}
    }

#ifdef WIN32
    WSACleanup();
#endif	// WIN32

    return ret_val;
};

