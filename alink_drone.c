#include <stdio.h>              // For printf, perror
#include <stdlib.h>             // For malloc, free, atoi
#include <string.h>             // For strtok, strdup
#include <unistd.h>             // For usleep
#include <pthread.h>            // For pthread functions
#include <sys/socket.h>         // For socket functions
#include <netinet/in.h>         // For sockaddr_in
#include <arpa/inet.h>          // For inet_pton
#include <stdbool.h>            // For bool, true, false
#include <sys/time.h>           // For timeval, settimeofday
#include <sys/wait.h>           // For waitpid
#include <time.h>               // For timespec, clock_gettime
#include <math.h>

#define BUFFER_SIZE 1024
#define DEFAULT_PORT 9999
#define DEFAULT_IP "10.5.0.10"
#define CONFIG_FILE "/etc/alink.conf"
#define PROFILE_FILE "/etc/txprofiles.conf"
#define MAX_PROFILES 20
#define DEFAULT_PACE_EXEC_MS 50

// Profile struct
typedef struct {
    int rangeMin;
    int rangeMax;
    char setGI[10];
    int setMCS;
    int setFecK;
    int setFecN;
    int setBitrate;
    float setGop;
    int wfbPower;
    char ROIqp[20];
	int bandwidth;
	int divideFpsBy;
} Profile;

Profile profiles[MAX_PROFILES];

// osd2udp struct
typedef struct {
    int udp_out_sock;
    char udp_out_ip[INET_ADDRSTRLEN];
    int udp_out_port;
} osd_udp_config_t;

// OSD strings
char global_regular_osd[64] = "initializing...";
char global_profile_osd[64] = "initializing...";
char global_gs_stats_osd[64] = "initializing...";
char global_extra_stats_osd[64] = "initializing...";

int global_fps = 120;

int prevWfbPower = -1;
float prevSetGop = -1.0;
int prevBandwidth = -20;
char prevSetGI[10] = "-1";
int prevSetMCS = -1;
int prevSetFecK = -1;
int prevSetFecN = -1;
int prevSetBitrate = -1;
char prevROIqp[20] = "-1";
int prevDivideFpsBy = -1;


long pace_exec = DEFAULT_PACE_EXEC_MS * 1000L;
int currentProfile = -1;
int previousProfile = -2;
long prevTimeStamp = 0;
float rssi_weight = 0.5;
float snr_weight = 0.5;
int hold_fallback_mode_s = 2;
int hold_modes_down_s = 2;
int min_between_changes_ms = 100;
int request_keyframe_interval_ms = 50;
bool allow_request_keyframe = 1;
bool allow_rq_kf_by_tx_d = 1;
int hysteresis_percent = 15;
int hysteresis_percent_down = 5;
int baseline_value = 100;
float smoothing_factor = 0.5;
float smoothing_factor_down = 0.8;
float smoothed_combined_value = 1500;

int applied_penalty = 0;
struct timespec penalty_timestamp;
int fec_rec_alarm = 20;
int fec_rec_penalty = 150;
int apply_penalty_for_s = 3;

int fallback_ms = 1000;
bool idr_every_change = false;
bool roi_focus_mode = false;

char fpsCommandTemplate[150], powerCommandTemplate[100], mcsCommandTemplate[100], bitrateCommandTemplate[150], gopCommandTemplate[100], fecCommandTemplate[100], roiCommandTemplate[150], idrCommandTemplate[100], msposdCommandTemplate[384];
bool verbose_mode = false;
bool selection_busy = false;
int message_count = 0;      // Global variable for message count
bool paused = false;        // Global variable for pause state
bool time_synced = false;   // Global flag to indicate if time has been synced
int last_value_sent = 100;
struct timespec last_exec_time;
struct timespec last_keyframe_request_time;	// Last keyframe request command
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for message count
pthread_mutex_t pause_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for pause state

#define MAX_CODES 5       // Maximum number of unique keyframe requests to track
#define CODE_LENGTH 8     // Max length of each unique code
#define EXPIRY_TIME_MS 1000 // Code expiry time in milliseconds

int total_keyframe_requests = 0;
long global_total_tx_dropped = 0;


// Struct to store each keyframe request code and its timestamp
typedef struct {
    char code[CODE_LENGTH];
    struct timespec timestamp;
} KeyframeRequest;

// Static array of keyframe requests
static KeyframeRequest keyframe_request_codes[MAX_CODES];
static int num_keyframe_requests = 0;  // Track the number of stored keyframe requests


long get_monotonic_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

void load_config(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open configuration file: %s\n", filename);
        perror("");
        exit(EXIT_FAILURE);
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        // Ignore comments (lines starting with '#')
        if (line[0] == '#') {
            continue;
        }

        char *key = strtok(line, "=");
        char *value = strtok(NULL, "\n");

        if (key && value) {
            if (strcmp(key, "rssi_weight") == 0) {
                rssi_weight = atof(value);
            } else if (strcmp(key, "snr_weight") == 0) {
                snr_weight = atof(value);
            } else if (strcmp(key, "hold_fallback_mode_s") == 0) {
                hold_fallback_mode_s = atoi(value);
            } else if (strcmp(key, "hold_modes_down_s") == 0) {
                hold_modes_down_s = atoi(value);
            } else if (strcmp(key, "min_between_changes_ms") == 0) {
                min_between_changes_ms = atoi(value);
			} else if (strcmp(key, "request_keyframe_interval_ms") == 0) {
                request_keyframe_interval_ms = atoi(value);
			} else if (strcmp(key, "fallback_ms") == 0) {
                fallback_ms = atoi(value);
			} else if (strcmp(key, "idr_every_change") == 0) {
                idr_every_change = atoi(value);
			} else if (strcmp(key, "allow_request_keyframe") == 0) {
                allow_request_keyframe = atoi(value);
			} else if (strcmp(key, "allow_rq_kf_by_tx_d") == 0) {
                allow_rq_kf_by_tx_d = atoi(value);
			} else if (strcmp(key, "hysteresis_percent") == 0) {
                hysteresis_percent = atoi(value);
			} else if (strcmp(key, "hysteresis_percent_down") == 0) {
                hysteresis_percent_down = atoi(value);
			} else if (strcmp(key, "exp_smoothing_factor") == 0) {
                smoothing_factor = atof(value);
			} else if (strcmp(key, "exp_smoothing_factor_down") == 0) {
                smoothing_factor_down = atof(value);
			} else if (strcmp(key, "roi_focus_mode") == 0) {
                roi_focus_mode = atoi(value);
			} else if (strcmp(key, "fec_rec_alarm") == 0) {
                fec_rec_alarm = atoi(value);
            } else if (strcmp(key, "fec_rec_penalty") == 0) {
                fec_rec_penalty = atoi(value);
            } else if (strcmp(key, "apply_penalty_for_s") == 0) {
                apply_penalty_for_s = atoi(value);
            } else if (strcmp(key, "powerCommand") == 0) {
                strncpy(powerCommandTemplate, value, sizeof(powerCommandTemplate));
			} else if (strcmp(key, "fpsCommandTemplate") == 0) {
                strncpy(fpsCommandTemplate, value, sizeof(fpsCommandTemplate));
            } else if (strcmp(key, "mcsCommand") == 0) {
                strncpy(mcsCommandTemplate, value, sizeof(mcsCommandTemplate));
            } else if (strcmp(key, "bitrateCommand") == 0) {
                strncpy(bitrateCommandTemplate, value, sizeof(bitrateCommandTemplate));
            } else if (strcmp(key, "gopCommand") == 0) {
                strncpy(gopCommandTemplate, value, sizeof(gopCommandTemplate));
            } else if (strcmp(key, "fecCommand") == 0) {
                strncpy(fecCommandTemplate, value, sizeof(fecCommandTemplate));
            } else if (strcmp(key, "roiCommand") == 0) {
                strncpy(roiCommandTemplate, value, sizeof(roiCommandTemplate));
            } else if (strcmp(key, "idrCommand") == 0) {
                strncpy(idrCommandTemplate, value, sizeof(idrCommandTemplate));

            } else if (strcmp(key, "msposdMSG") == 0) {
                strncpy(global_regular_osd, value, sizeof(global_regular_osd));

            } else {
                fprintf(stderr, "Warning: Unrecognized configuration key: %s\n", key);
				exit(EXIT_FAILURE);  // Exit the program with an error status
            }
        } else if (strlen(line) > 1 && line[0] != '\n') {  // ignore empty lines
            fprintf(stderr, "Error: Invalid configuration format: %s\n", line);
            exit(EXIT_FAILURE);
        }
    }

    fclose(file);
}


void load_profiles(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Problem loading %s: ", filename);
        perror(""); // Print system error message
        exit(1);
    }

    char line[256];
    int i = 0;

    while (fgets(line, sizeof(line), file) && i < MAX_PROFILES) {
        // Skip comments
        char *comment = strchr(line, '#');
        if (comment) {
            *comment = '\0'; // Truncate the line at the comment
        }

        // Trim trailing whitespace and newlines
        line[strcspn(line, "\r\n")] = '\0';

        // Skip empty or whitespace-only lines
        if (strlen(line) == 0) {
            continue;
        }

        // Parse the line
        if (sscanf(line, "%d - %d %s %d %d %d %d %f %d %s %d %d",
                   &profiles[i].rangeMin, &profiles[i].rangeMax, profiles[i].setGI,
                   &profiles[i].setMCS, &profiles[i].setFecK, &profiles[i].setFecN,
                   &profiles[i].setBitrate, &profiles[i].setGop, &profiles[i].wfbPower,
                   profiles[i].ROIqp, &profiles[i].bandwidth, &profiles[i].divideFpsBy) == 12) {
            i++;
        } else {
            fprintf(stderr, "Malformed line ignored: %s\n", line);
        }
    }

    fclose(file);
}


// Function to read fps from majestic.yaml
int get_video_fps() {
    char command[] = "cli --get .video0.fps";
    char buffer[128]; // Buffer to store command output
    FILE *pipe;
    int fps = 0;

    // Open a pipe to execute the command
    pipe = popen(command, "r");
    if (pipe == NULL) {
        fprintf(stderr, "Failed to run cli --get .video0.fps\n");
        return -1; // Return an error code
    }

    // Read the output from the command
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        // Convert the output string to an integer
        fps = atoi(buffer);
    }

    // Close the pipe
    pclose(pipe);

    return fps;
}

// Function to setup roi in majestic.yaml based on resolution
int setup_roi() {

    // Variables for resolution
    int x_res, y_res;
    char resolution[32];

    // Execute system command to get resolution and store it in a file
    FILE *fp = popen("cli --get .video0.size", "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        return 1;
    }

    // Read the output of the command
    fgets(resolution, sizeof(resolution) - 1, fp);
    pclose(fp);

    // Parse the resolution in the format <x_res>x<y_res>
    if (sscanf(resolution, "%dx%d", &x_res, &y_res) != 2) {
        printf("Failed to parse resolution\n");
        return 1;
    }

    // Round x_res and y_res to nearest multiples of 32
    int rounded_x_res = floor(x_res / 32) * 32;
    int rounded_y_res = floor(y_res / 32) * 32;

    // ROI calculation with additional condition
    int roi_height, start_roi_y;
    if (rounded_y_res != y_res) {
        roi_height = rounded_y_res - 32;
        start_roi_y = 32;
    } else {
        roi_height = rounded_y_res;
        start_roi_y = y_res - rounded_y_res;
    }

    // Make rois 32 lower for clear stats, make total roi 32 less
    roi_height = roi_height - 32;
    start_roi_y = start_roi_y + 32;

    // Calculate edge_roi_width and next_roi_width as multiples of 32
    int edge_roi_width = floor(rounded_x_res / 8 / 32) * 32;
    int next_roi_width = (floor(rounded_x_res / 8 / 32) * 32) + 32;

    int coord0 = 0;
    int coord1 = edge_roi_width;
    int coord2 = x_res - edge_roi_width - next_roi_width;
    int coord3 = x_res - edge_roi_width;

    // Format ROI definition as a string
    char roi_define[256];
    snprintf(roi_define, sizeof(roi_define), "%dx%dx%dx%d,%dx%dx%dx%d,%dx%dx%dx%d,%dx%dx%dx%d",
             coord0, start_roi_y, edge_roi_width, roi_height,
             coord1, start_roi_y, next_roi_width, roi_height,
             coord2, start_roi_y, next_roi_width, roi_height,
             coord3, start_roi_y, edge_roi_width, roi_height);

    // Prepare the command to set ROI
    char command[512];
    snprintf(command, sizeof(command), "cli --set .fpv.roiRect %s", roi_define);

    // Check if .fpv.enabled is set
    char enabled_status[16];
    fp = popen("cli --get .fpv.enabled", "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        return 1;
    }

    fgets(enabled_status, sizeof(enabled_status) - 1, fp);
    pclose(fp);

    // Trim newline character
    enabled_status[strcspn(enabled_status, "\n")] = 0;

    // Check if enabled_status is "true" or "false"
    if (strcmp(enabled_status, "true") != 0 && strcmp(enabled_status, "false") != 0) {
        system("cli --set .fpv.enabled true");
    }

    // Run the command to set ROI
    system(command);

    // Check if .fpv.roiQp is set correctly
    char roi_qp_status[32];
    fp = popen("cli --get .fpv.roiQp", "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        return 1;
    }

    fgets(roi_qp_status, sizeof(roi_qp_status) - 1, fp);
    pclose(fp);

    // Trim newline character
    roi_qp_status[strcspn(roi_qp_status, "\n")] = 0;

    // Check for four integers separated by commas
    int num_count = 0;
    char *token = strtok(roi_qp_status, ",");
    while (token != NULL) {
        num_count++;
        token = strtok(NULL, ",");
    }

    if (num_count != 4) {
        system("cli --set .fpv.roiQp 0,0,0,0");
    }

    return 0;
}




// Get the profile based on input value
Profile* get_profile(int input_value) {
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (input_value >= profiles[i].rangeMin && input_value <= profiles[i].rangeMax) {
            return &profiles[i];
        }
    }
    return NULL;
}

// Execute system command without adding quotes
void execute_command_no_quotes(const char* command) {
    if (verbose_mode) {
        puts(command);
    }
	system(command);

}

// Execute command, add quotes first
void execute_command(const char* command) {
    // Create a new command with quotes
    char quotedCommand[BUFFER_SIZE]; // Define a buffer for the quoted command
    snprintf(quotedCommand, sizeof(quotedCommand), "\"%s\"", command); // Add quotes around the command
    if (verbose_mode) {
        puts(quotedCommand);
    }
	system(quotedCommand);
	if (verbose_mode) {
		printf("Waiting %ldms\n", pace_exec / 1000);
    }
	usleep(pace_exec);
}


void apply_profile(Profile* profile) {

    char powerCommand[100];
	char fpsCommand[150];
    char mcsCommand[150];
    char bitrateCommand[150];
    char gopCommand[100];
    char fecCommand[100];
    char roiCommand[150];
    const char *idrCommand;
    //char msposdCommand[512];

    idrCommand = idrCommandTemplate;

	// Calculate seconds since last change
    long currentTime = get_monotonic_time();
    long timeElapsed = currentTime - prevTimeStamp; // Time since the last change


    // Load current profile variables into local variables
    int currentWfbPower = profile->wfbPower;
    float currentSetGop = profile->setGop;
    char currentSetGI[10];
    strcpy(currentSetGI, profile->setGI);
    int currentSetMCS = profile->setMCS;
    int currentSetFecK = profile->setFecK;
    int currentSetFecN = profile->setFecN;
    int currentSetBitrate = profile->setBitrate;
    char currentROIqp[20];
    strcpy(currentROIqp, profile->ROIqp);
	int currentBandwidth = profile->bandwidth;
    int currentDivideFpsBy = profile->divideFpsBy;

    // Logic to determine execution order and see if values are different
    if (currentProfile > previousProfile) {

		if (currentDivideFpsBy != prevDivideFpsBy) {
            sprintf(fpsCommand, fpsCommandTemplate, global_fps / currentDivideFpsBy);
            execute_command(fpsCommand);
            prevDivideFpsBy = currentDivideFpsBy;
		}

        if (currentWfbPower != prevWfbPower) {
            sprintf(powerCommand, powerCommandTemplate, currentWfbPower * 50);
            execute_command(powerCommand);
            prevWfbPower = currentWfbPower;
        }
        if (currentSetGop != prevSetGop) {
            sprintf(gopCommand, gopCommandTemplate, currentSetGop);
            execute_command(gopCommand);
            prevSetGop = currentSetGop;
        }
		
		if (strcmp(currentSetGI, prevSetGI) != 0 || currentSetMCS != prevSetMCS || currentBandwidth != prevBandwidth) {
            sprintf(mcsCommand, mcsCommandTemplate, currentBandwidth, currentSetGI, currentSetMCS);
            execute_command(mcsCommand);
			prevBandwidth = currentBandwidth;
            strcpy(prevSetGI, currentSetGI);
            prevSetMCS = currentSetMCS;
        }
        if (currentSetFecK != prevSetFecK || currentSetFecN != prevSetFecN) {
            sprintf(fecCommand, fecCommandTemplate, currentSetFecK, currentSetFecN);
            execute_command(fecCommand);
            prevSetFecK = currentSetFecK;
            prevSetFecN = currentSetFecN;
        }
        if (currentSetBitrate != prevSetBitrate) {
            sprintf(bitrateCommand, bitrateCommandTemplate, currentSetBitrate * currentDivideFpsBy);
            execute_command(bitrateCommand);
            prevSetBitrate = currentSetBitrate;
        }
        if (roi_focus_mode && strcmp(currentROIqp, prevROIqp) != 0) {
            sprintf(roiCommand, roiCommandTemplate, currentROIqp);
            execute_command(roiCommand);
            strcpy(prevROIqp, currentROIqp);
        }
		if (idr_every_change) {
			execute_command(idrCommand);
		}

    } else {

		if (currentDivideFpsBy != prevDivideFpsBy) {
            sprintf(fpsCommand, fpsCommandTemplate, global_fps / currentDivideFpsBy);
            execute_command(fpsCommand);
            prevDivideFpsBy = currentDivideFpsBy;
		}

        if (currentSetBitrate != prevSetBitrate) {
            sprintf(bitrateCommand, bitrateCommandTemplate, currentSetBitrate * currentDivideFpsBy);
            execute_command(bitrateCommand);
            prevSetBitrate = currentSetBitrate;
        }
        if (currentSetGop != prevSetGop) {
            sprintf(gopCommand, gopCommandTemplate, currentSetGop);
            execute_command(gopCommand);
            prevSetGop = currentSetGop;
        }
        if (strcmp(currentSetGI, prevSetGI) != 0 || currentSetMCS != prevSetMCS || currentBandwidth != prevBandwidth) {
            sprintf(mcsCommand, mcsCommandTemplate, currentBandwidth, currentSetGI, currentSetMCS);
            execute_command(mcsCommand);
			prevBandwidth = currentBandwidth;
            strcpy(prevSetGI, currentSetGI);
            prevSetMCS = currentSetMCS;
        }
        if (currentSetFecK != prevSetFecK || currentSetFecN != prevSetFecN) {
            sprintf(fecCommand, fecCommandTemplate, currentSetFecK, currentSetFecN);
            execute_command(fecCommand);
            prevSetFecK = currentSetFecK;
            prevSetFecN = currentSetFecN;
        }
        if (currentWfbPower != prevWfbPower) {
            sprintf(powerCommand, powerCommandTemplate, currentWfbPower * 50);
            execute_command(powerCommand);
            prevWfbPower = currentWfbPower;
        }
        if (roi_focus_mode && strcmp(currentROIqp, prevROIqp) != 0) {
            sprintf(roiCommand, roiCommandTemplate, currentROIqp);
            execute_command(roiCommand);
            strcpy(prevROIqp, currentROIqp);
        }

		if (idr_every_change) {
			execute_command(idrCommand);
		}
    }

	// Generate string with profile stats for osd
	sprintf(global_profile_osd, "%lds %d %s%d %d/%d Pw%d g%.1f", 
        timeElapsed, 
        profile->setBitrate, 
        profile->setGI, 
        profile->setMCS, 
        profile->setFecK,
        profile->setFecN, 
        profile->wfbPower, 
        profile->setGop);
}

void *periodic_update_osd(void *arg) {
    osd_udp_config_t *osd_config = (osd_udp_config_t *)arg;

    struct sockaddr_in udp_out_addr;
    if (osd_config->udp_out_sock != -1) {
        // Initialize the target address for UDP
        memset(&udp_out_addr, 0, sizeof(udp_out_addr));
        udp_out_addr.sin_family = AF_INET;
        udp_out_addr.sin_port = htons(osd_config->udp_out_port);
        if (inet_pton(AF_INET, osd_config->udp_out_ip, &udp_out_addr.sin_addr) <= 0) {
            perror("Invalid IP address for OSD UDP output");
            pthread_exit(NULL);
        }
    }

    while (true) {
        sleep(1);

        // Generate string with extra OSD stats to combine with other strings
        snprintf(global_extra_stats_osd, sizeof(global_extra_stats_osd), "pnlt%d xtx%ld idr%d",
                 applied_penalty,
				 global_total_tx_dropped,
				 total_keyframe_requests);

        // Combine all the strings: profile stats + regular msposd string + gs stats + extra stats
        char full_osd_string[256];
        snprintf(full_osd_string, sizeof(full_osd_string), "%s %s %s %s",
                 global_profile_osd, global_regular_osd, global_gs_stats_osd, global_extra_stats_osd);
        
		// Either update OSD remotely over udp, or update local file
		if (osd_config->udp_out_sock != -1) {
            // Send the OSD string over UDP
            ssize_t sent_bytes = sendto(osd_config->udp_out_sock, full_osd_string, strlen(full_osd_string), 0,
                                        (struct sockaddr *)&udp_out_addr, sizeof(udp_out_addr));
            if (sent_bytes < 0) {
                perror("Error sending OSD string over UDP");
            }
        } else {
            // Write to /tmp/MSPOSD.msg
            FILE *file = fopen("/tmp/MSPOSD.msg", "w");
            if (file == NULL) {
                perror("Error opening /tmp/MSPOSD.msg");
                continue; // Skip this iteration if the file cannot be opened
            }

            if (fwrite(full_osd_string, sizeof(char), strlen(full_osd_string), file) != strlen(full_osd_string)) {
                perror("Error writing to /tmp/MSPOSD.msg");
            }

            fclose(file);
        }
    }
    return NULL;
}



bool value_chooses_profile(int input_value) {
    // Get the appropriate profile based on input
    Profile* selectedProfile = get_profile(input_value);
    if (selectedProfile == NULL) {
        printf("No matching profile found for input: %d\n", input_value);
		return false;
    }

    // Find the index of the selected profile

    for (int i = 0; i < MAX_PROFILES; i++) {
        if (selectedProfile == &profiles[i]) {
            currentProfile = i;
            break;
        }
    }

    // If the previous profile is the same, do not apply changes
    if (previousProfile == currentProfile) {
        printf("No change: Link value is within same profile.\n");
		return false;
    }

    // Check if a change is needed based on time constraints
    long currentTime = get_monotonic_time();
    long timeElapsed = currentTime - prevTimeStamp;

    if (previousProfile == 0 && timeElapsed <= hold_fallback_mode_s) {
        if (verbose_mode) {
			puts("Holding fallback...");
		}
		return false;
    }
    if (previousProfile < currentProfile && timeElapsed <= hold_modes_down_s) {
        if (verbose_mode) {
			puts("Holding mode down...");
		}
		return false;
    }

    // Apply the selected profile
    apply_profile(selectedProfile);
	// Update previousProfile
    previousProfile = currentProfile;
	prevTimeStamp = currentTime;
	return true;

}

void start_selection(int rssi_score, int snr_score, int recovered) {

    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    // Shortcut for fallback profile 999
    if (rssi_score == 999) {
        if (value_chooses_profile(999)) {
            printf("Applied.\n");
            last_value_sent = 999;
            smoothed_combined_value = 999;
            last_exec_time = current_time;
        } else {
            printf("Not applied.\n");
		}
		return;
    }

	// Check for any penalties
	if (recovered >= fec_rec_alarm && (fec_rec_penalty * recovered) > applied_penalty) {
		applied_penalty = fec_rec_penalty * recovered;
		penalty_timestamp = current_time;
		if (verbose_mode) {
			puts("fec_rec penalty condition met...");
		}
	}
	// Check penalty expiration
    if (applied_penalty > 0 && (current_time.tv_sec - penalty_timestamp.tv_sec) > apply_penalty_for_s) {
        applied_penalty = 0;
        if (verbose_mode) {
            puts("fec_rec penalty time is up. Penalty withdrawn");
        }
    }

	if (selection_busy) {
        if (verbose_mode) {
			puts("Selection process busy...");
		}
		return;
    }
    selection_busy = true;

    // Combine rssi and snr by weight
	float combined_value_float = rssi_score * rssi_weight + snr_score * snr_weight;

	// Deduct penalty if it is active
	if (applied_penalty > 0) {
		combined_value_float -= applied_penalty;
		if (verbose_mode) {
			printf("Deducting penalty of %d from current link score, new combined value: %.2f\n", applied_penalty, combined_value_float);
		}
	}

	// Determine which exp_smoothing_factor to use (up or down)
    float chosen_smoothing_factor = (combined_value_float >= last_value_sent) ? smoothing_factor : smoothing_factor_down;

	// Apply exponential smoothing
    smoothed_combined_value = (chosen_smoothing_factor * combined_value_float + (1 - chosen_smoothing_factor) * smoothed_combined_value);

	// Check if enough time has passed
    long time_diff_ms = (current_time.tv_sec - last_exec_time.tv_sec) * 1000 + (current_time.tv_nsec - last_exec_time.tv_nsec) / 1000000;
    if (time_diff_ms < min_between_changes_ms) {
        printf("Skipping profile load: time_diff_ms=%ldms - too soon (min %dms required)\n", time_diff_ms, min_between_changes_ms);
        selection_busy = false;
        return;
    }
	// Clamp combined value within the defined range
    int combined_value = (int)floor(smoothed_combined_value);
    combined_value = (combined_value < 1000) ? 1000 : (combined_value > 2000) ? 2000 : combined_value;

    // Calculate percentage change from smoothed baseline value
    float percent_change = fabs((float)(combined_value - last_value_sent) / last_value_sent) * 100;

    // Determine which hysteresis threshold to use (up or down)
    float hysteresis_threshold = (combined_value >= last_value_sent) ? hysteresis_percent : hysteresis_percent_down;

    // Check if the change exceeds the chosen hysteresis threshold
    if (percent_change >= hysteresis_threshold) {
        printf("Qualified to request profile: %d is > %.2f%% different (%.2f%%)\n", combined_value, hysteresis_threshold, percent_change);

        // Request profile, check if applied
        if (value_chooses_profile(combined_value)) {
            printf("Profile %d applied.\n", combined_value);
            last_value_sent = combined_value;
            //smoothed_combined_value = (float)combined_value;  // Update baseline for future comparisons - Should be redundant, as this value already came from that
            last_exec_time = current_time;
        }
    }
    selection_busy = false;
}


// request_keyframe function to check if a code exists in the array and has not expired
bool code_exists(const char *code, struct timespec *current_time) {
    for (int i = 0; i < num_keyframe_requests; i++) {
        if (strcmp(keyframe_request_codes[i].code, code) == 0) {
            // Check if the request is still valid
            long elapsed_time_ms = (current_time->tv_sec - keyframe_request_codes[i].timestamp.tv_sec) * 1000 +
                                   (current_time->tv_nsec - keyframe_request_codes[i].timestamp.tv_nsec) / 1000000;
            if (elapsed_time_ms < EXPIRY_TIME_MS) {
                return true;  // Code exists and has not expired
            } else {
                // Expired: Remove it by shifting the rest down
                memmove(&keyframe_request_codes[i], &keyframe_request_codes[i + 1],
                        (num_keyframe_requests - i - 1) * sizeof(KeyframeRequest));
                num_keyframe_requests--;
				i--;  // Adjust index to re-check at this position after shift
                return false;  // Code expired
            }
        }
    }
    return false;  // Code not found
}

// Function to add a code to the array
void add_code(const char *code, struct timespec *current_time) {
    if (num_keyframe_requests < MAX_CODES) {
        strncpy(keyframe_request_codes[num_keyframe_requests].code, code, CODE_LENGTH);
        keyframe_request_codes[num_keyframe_requests].timestamp = *current_time;
        num_keyframe_requests++;
    } else {
        printf("Max keyframe request codes reached. Consider increasing MAX_CODES.\n");
    }
}

void cleanup_expired_codes(struct timespec *current_time) {
    for (int i = 0; i < num_keyframe_requests; ) {
        // Calculate elapsed time in milliseconds
        long elapsed_time_ms = (current_time->tv_sec - keyframe_request_codes[i].timestamp.tv_sec) * 1000 +
                               (current_time->tv_nsec - keyframe_request_codes[i].timestamp.tv_nsec) / 1000000;

        // Remove the expired entry if elapsed time exceeds expiry threshold
        if (elapsed_time_ms >= EXPIRY_TIME_MS) {
            memmove(&keyframe_request_codes[i], &keyframe_request_codes[i + 1],
                    (num_keyframe_requests - i - 1) * sizeof(KeyframeRequest));
            num_keyframe_requests--;  // Decrease the count of requests
        } else {
            i++;  // Only move to the next entry if no removal
        }
    }
}

// Main function to handle special commands
void special_command_message(const char *msg) {
    const char *cleaned_msg = msg + 8;  // Skip "special:"
    const char *idrCommand = idrCommandTemplate;

    char *separator = strchr(cleaned_msg, ':');
    char code[CODE_LENGTH] = {0};  // Buffer for unique request code

    if (separator) {
        *separator = '\0';  // Split at the first ':'
        strncpy(code, separator + 1, CODE_LENGTH - 1);  // Copy unique code if present
    }

    // Check for keyframe request first
    if (allow_request_keyframe && prevSetGop > 0.5 && strcmp(cleaned_msg, "request_keyframe") == 0 && code[0] != '\0') {
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        
        // Clean up expired codes before proceeding
        cleanup_expired_codes(&current_time);

        // Check if the keyframe request interval has elapsed
        long elapsed_ms = (current_time.tv_sec - last_keyframe_request_time.tv_sec) * 1000 +
                          (current_time.tv_nsec - last_keyframe_request_time.tv_nsec) / 1000000;
        
        if (elapsed_ms >= request_keyframe_interval_ms) {
            if (!code_exists(code, &current_time)) {
                add_code(code, &current_time);  // Store new code and timestamp

                // Request new keyframe
                char quotedCommand[BUFFER_SIZE];
                snprintf(quotedCommand, sizeof(quotedCommand), "\"%s\"", idrCommand);
                if (verbose_mode) {
                    printf("Special: Requesting Keyframe for code: %s\n", code);
                }
                system(quotedCommand);
                last_keyframe_request_time = current_time;
				total_keyframe_requests++;
            } else {
                if (verbose_mode) {
					printf("Already requested keyframe for code: %s\n", code);
				}
			}
        } else {
                if (verbose_mode) {
					printf("Keyframe request ignored. Interval not met for code: %s\n", code);
				}
        }

    } else if (strcmp(cleaned_msg, "pause_adaptive") == 0) {
        pthread_mutex_lock(&pause_mutex);
        paused = true;
        pthread_mutex_unlock(&pause_mutex);
        printf("Paused adaptive mode\n");

    } else if (strcmp(cleaned_msg, "resume_adaptive") == 0) {
        pthread_mutex_lock(&pause_mutex);
        paused = false;
        pthread_mutex_unlock(&pause_mutex);
        printf("Resumed adaptive mode\n");

    } else {
        printf("Unknown or disabled special command: %s\n", cleaned_msg);
    }
}


//function to get latest tx dropped
long get_wlan0_tx_dropped() {
    FILE *fp;
    char line[256];
    long tx_dropped = 0;

    // Open the /proc/net/dev file
    fp = fopen("/proc/net/dev", "r");
    if (fp == NULL) {
        perror("Failed to open /proc/net/dev");
        return -1;
    }

    // Skip the first two lines (headers)
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);

    // Read each line to find the wlan0 interface
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "wlan0:") != NULL) {
            // Locate the stats after the "wlan0:" label
            char *stats_str = strchr(line, ':');
            if (stats_str) {
                stats_str++;  // Move past the colon

                // Tokenize to skip to the 12th field
                char *token;
                int field_count = 0;
                token = strtok(stats_str, " ");

                while (token != NULL) {
                    field_count++;
                    if (field_count == 12) {
                        tx_dropped = strtol(token, NULL, 10);
                        break;
                    }
                    token = strtok(NULL, " ");
                }
            }
            break;
        }
    }

    // Close the file
    fclose(fp);
	
    //calculate difference
	long latest_tx_dropped = tx_dropped - global_total_tx_dropped;
	//update global total
	global_total_tx_dropped = tx_dropped;
	return latest_tx_dropped;

}

void *periodic_tx_dropped(void *arg) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyz"; // Lowercase letters
    static int seeded = 0;

    while (1) {
        long latest_tx_dropped = get_wlan0_tx_dropped();

        if (allow_rq_kf_by_tx_d && prevSetGop > 0.5 && latest_tx_dropped > 0) {
         
            // Seed the random number generator (once per program execution)
            if (!seeded) {
                srand((unsigned int)time(NULL));
                seeded = 1;
            }

            // Generate a random string of lowercase letters
            char random_string[4 + 1];
            for (int i = 0; i < 4; i++) {
                random_string[i] = charset[rand() % (sizeof(charset) - 1)];
            }
            random_string[4] = '\0'; // Null terminator

            // Create the keyframe request string
            char keyframe_request[64];
            snprintf(keyframe_request, sizeof(keyframe_request), "special:request_keyframe:%s", random_string);

            // Pass the generated string to the second function
            special_command_message(keyframe_request);

            if (verbose_mode) {
                printf("Requesting keyframe for locally dropped tx packet\n");
            }

        }

        // Sleep for 100 milliseconds (100000 microseconds)
        usleep(100000);  
    }
}

void *count_messages(void *arg) {
    int local_count;
    while (1) {
        usleep(fallback_ms * 1000);
        pthread_mutex_lock(&count_mutex);
        local_count = message_count;
        message_count = 0;  //reset count
        pthread_mutex_unlock(&count_mutex);

        pthread_mutex_lock(&pause_mutex);
        if (local_count == 0 && !paused) {
            printf("No messages received in %dms, sending 999\n", fallback_ms);
            start_selection(999, 1000, 0);
		} else {

			if (verbose_mode) {
				printf("Messages per %dms: %d\n", fallback_ms, local_count);
			}
        }
        pthread_mutex_unlock(&pause_mutex);
    }
    return NULL;
}

void process_message(const char *msg) {

	// Declare  default local variables
    struct timeval tv;
    int transmitted_time = 0;
    int link_value_rssi = 999;
    int link_value_snr = 999;
	int recovered = 0;
    int recovered_otime = 0;
    int rssi1 = -105;
    int snr1 = 0;
    int rssi3 = -105;
    int rssi4 = -105;

    // Copy the input string to avoid modifying the original
    char *msgCopy = strdup(msg);
    if (msgCopy == NULL) {
        perror("Failed to allocate memory");
        return;
    }

    // Use strtok to split the string by ':'
    char *token = strtok(msgCopy, ":");
    int index = 0;

    // Iterate through tokens and convert to integers
    while (token != NULL) {
        switch (index) {
            case 0:
                transmitted_time = atoi(token);
                break;
            case 1:
                link_value_rssi = atoi(token);
				break;
			case 2:
                link_value_snr = atoi(token);
                break;
            case 3:
                recovered = atoi(token);
                break;
            case 4:
                recovered_otime = atoi(token);
                break;
            case 5:
                rssi1 = atoi(token);
                break;
            case 6:
                snr1 = atoi(token);
                break;
            case 7:
                rssi3 = atoi(token);
                break;
            case 8:
                rssi4 = atoi(token);
                break;
            default:
                // Ignore extra tokens
                break;
        }
        token = strtok(NULL, ":");
        index++;
    }

    // Free the duplicated string
    free(msgCopy);

	// Create osd string with gs information
	sprintf(global_gs_stats_osd, "rssi%d, snr%d fec%d",
				rssi1,
				snr1,
				recovered);

	// Only proceed with time synchronization if it hasn't been set yet
    if (!time_synced) {
        if (transmitted_time > 0) {
            tv.tv_sec = transmitted_time;
            tv.tv_usec = 0;
            if (settimeofday(&tv, NULL) == 0) {
                printf("System time synchronized with transmitted time: %ld\n", (long)transmitted_time);
                time_synced = true;
            } else {
                perror("Failed to set system time");
            }
        }
	}

    // Start selection if not paused
    pthread_mutex_lock(&pause_mutex);
    if (!paused) {
        start_selection(link_value_rssi, link_value_snr, recovered);
    } else {
        printf("Adaptive mode paused, waiting for resume command...\n");
    }
        pthread_mutex_unlock(&pause_mutex);

}


void print_usage() {
    printf("Usage: ./udp_server --port <port> --pace-exec <time> --verbose\n");
    printf("Options:\n");
	printf("  --ip         IP address to bind to (default: %s)\n", DEFAULT_IP);
    printf("  --port       Port to listen on (default: %d)\n", DEFAULT_PORT);
    printf("  --verbose    Enable verbose output\n");
    printf("  --pace-exec  Maj/wfb control execution pacing interval in milliseconds (default: %d ms)\n", DEFAULT_PACE_EXEC_MS);
}

int main(int argc, char *argv[]) {
    load_config(CONFIG_FILE);
    load_profiles(PROFILE_FILE);

    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int port = DEFAULT_PORT;
    char ip[INET_ADDRSTRLEN] = DEFAULT_IP; // Default IP

    // Initialize osd_udp_config_t struct
    osd_udp_config_t osd_config = { .udp_out_sock = -1 };

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--ip") == 0 && i + 1 < argc) {
            strncpy(ip, argv[++i], INET_ADDRSTRLEN);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose_mode = true;
        } else if (strcmp(argv[i], "--pace-exec") == 0 && i + 1 < argc) {
            int ms = atoi(argv[++i]);
            pace_exec = ms * 1000L; // Convert milliseconds to microseconds

        } else if (strcmp(argv[i], "--osd2udp") == 0 && i + 1 < argc) {
            char *ip_port = argv[++i];
            char *colon_pos = strchr(ip_port, ':');
            if (colon_pos) {
                *colon_pos = '\0'; // Split IP and port
                strncpy(osd_config.udp_out_ip, ip_port, INET_ADDRSTRLEN);
                osd_config.udp_out_port = atoi(colon_pos + 1);
            } else {
                fprintf(stderr, "Invalid format for --osd2udp. Expected <ip:port>\n");
                return 1;
            }

            // Create the outgoing UDP socket
            if ((osd_config.udp_out_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                perror("Error creating outgoing UDP socket");
                return 1;
            }

            printf("OSD UDP output enabled to %s:%d\n", osd_config.udp_out_ip, osd_config.udp_out_port);
        } else {
            print_usage();
            return 1;
        }
    }

    // Create UDP socket for incoming messages
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Bind the socket
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Listening on UDP port %d, IP: %s...\n", port, ip);
	
	//Get fps value from majestic
	int fps = get_video_fps();
    if (fps >= 0) {
        printf("Video FPS: %d\n", fps);
		global_fps = fps;
    } else {
        printf("Failed to retrieve video FPS from majestic.\n");
    }

    // Check if roi_focus_mode is enabled and call the setup_roi function
    if (roi_focus_mode) {
        if (setup_roi() != 0) {
            printf("Failed to set up focus mode regions based on majestic resolution. You may have to do it manually.\n");
        } else {
            printf("Focus mode regions set in majestic.yaml\n");
        }
    }

    // Start the counting thread
    pthread_t count_thread;
    pthread_create(&count_thread, NULL, count_messages, NULL);

    // Start the periodic OSD update thread, passing osd_config
    pthread_t osd_thread;
    pthread_create(&osd_thread, NULL, periodic_update_osd, &osd_config);

    // Start the periodic TX dropped thread
    pthread_t tx_dropped_thread;
    pthread_create(&tx_dropped_thread, NULL, periodic_tx_dropped, NULL);

    // Main loop for processing incoming messages
    while (1) {
        // Receive a message
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr *)&client_addr, &client_addr_len);
        if (n < 0) {
            perror("recvfrom failed");
            break;
        }

        // Increment message count
        pthread_mutex_lock(&count_mutex);
        message_count++;
        pthread_mutex_unlock(&count_mutex);

        // Null-terminate the received data
        buffer[n] = '\0';

        // Extract the length of the message (first 4 bytes)
        uint32_t msg_length;
        memcpy(&msg_length, buffer, sizeof(msg_length));
        msg_length = ntohl(msg_length); // Convert from network to host byte order

        // Print the message length and content
        if (verbose_mode) {
            printf("Received message (%u bytes): %s\n", msg_length, buffer + sizeof(msg_length));
        }

        // Strip length off the start of the message
        char *message = buffer + sizeof(uint32_t);
        // See if it's a special command, otherwise process it
        if (strncmp(message, "special:", 8) == 0) {
            special_command_message(message);
        } else {
            process_message(message);
        }
    }

    // Close the socket
    close(sockfd);

    // Close outgoing OSD socket if it was opened
    if (osd_config.udp_out_sock != -1) {
        close(osd_config.udp_out_sock);
    }

    return 0;
}


