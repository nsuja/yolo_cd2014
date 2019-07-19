#include <darknet.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define THREAD_COUNT (1)

void * process_image(void* arg);

typedef struct {
	char **names;
	image **alphabet;
	network *net;
	float thresh;
	float hier_thresh;
	int isdir;
	char *filename;

	char d_name[256];
	char input[256];
}Process_Image_Thread_Args;

int is_directory(const char *path) {
	struct stat statbuf;
	if (stat(path, &statbuf) != 0)
		return 0;
	return S_ISDIR(statbuf.st_mode);
}

void test_detector(char *datacfg, char *cfgfile, char *weightfile, char *filename, float thresh, float hier_thresh, int threads)
{
	pthread_t thread[THREAD_COUNT];
	int free_thread[THREAD_COUNT] = {[0 ... THREAD_COUNT-1] = 1};
	int running_threads = 0;
    list *options = read_data_cfg(datacfg);
    char *name_list = option_find_str(options, "names", "data/names.list");
    char **names = get_labels(name_list);
	int i;
	network *net[THREAD_COUNT];
	printf("%s:: Cargo las redes (%d)", __func__, THREAD_COUNT);
	for(i = 0; i < THREAD_COUNT; i++) {
		net[i] = load_network(cfgfile, weightfile, 0);
		set_batch_network(net[i], 1);
	}


    image **alphabet = load_alphabet();
    srand(2222222);
    char buff[256];
    char *input = buff;
	DIR *d = NULL;
	struct dirent *dir;
	int isdir = 0;
	printf("Filename: %s\n", filename);
	fflush(stdout);
	if(is_directory(filename)){
		isdir = 1;
	}

	while(1){
		for(i = 0; i < THREAD_COUNT; i++) {
			void *retval;
			if(free_thread[i] == 0 && pthread_tryjoin_np(thread[i], &retval) == 0) {
				running_threads --;
				free_thread[i] = 1;
			}
		}
		if(running_threads >= THREAD_COUNT) {
			usleep(100000);
			continue;
		}
		if(isdir){
			if(!d)
				d = opendir(filename);
			if (d) {
				dir = readdir(d);
				if(dir == NULL) {
					printf("Termine de leer errno(%d) %s\n", errno, strerror(errno));
					break;
				}
				if(strstr(dir->d_name, "jpg")) {
					printf("Cargo: %s\n", dir->d_name);
					fflush(stdout);
					strncpy(input, dir->d_name, 256);
					sprintf(input, "%s/%s", filename, dir->d_name);
				} else {
					continue;
				}
			} else {
				printf("Dir invalido? errno(%d) %s\n", errno, strerror(errno));
				break;
			}
		}else if(filename){
			printf("Cargo: %s\n", filename);
            fflush(stdout);
            strncpy(input, filename, 256);
        } else {
            printf("Enter Image Path: ");
            fflush(stdout);
            input = fgets(input, 256, stdin);
            if(!input) return;
            strtok(input, "\n");
        }
		Process_Image_Thread_Args *thread_args;
		thread_args = (Process_Image_Thread_Args*)calloc(1, sizeof(Process_Image_Thread_Args));
		thread_args->names = names;
		thread_args->alphabet = alphabet;
		thread_args->net = net[0];
		thread_args->thresh = thresh;
		thread_args->hier_thresh = hier_thresh;
		thread_args->isdir = isdir;
		thread_args->filename = filename;
		if(isdir)
			sprintf(thread_args->d_name,"%s", dir->d_name);
		sprintf(thread_args->input,"%s", input);

		for(i = 0; i < THREAD_COUNT; i++) {
			if(free_thread[i] == 1)
				break;
		}
		if(i >= THREAD_COUNT) {
			printf("Paso algo raro! Salgo!\n");
			exit(1);
		}
		pthread_create(&thread[i], NULL, process_image, (void*)thread_args);
		free_thread[i] = 0;
		running_threads ++;
		if(!isdir) {
			pthread_join(thread[i], NULL);
			break;
		}
    }
	closedir(d);
}

void * process_image(void* arg)
{
	Process_Image_Thread_Args *args = (Process_Image_Thread_Args*) arg;
	char **names = args->names;
	network *net = args->net;
	image **alphabet = args->alphabet;
	float thresh = args->thresh;
	float hier_thresh = args->hier_thresh;
	int isdir = args->isdir;
	char *input = args->input;
	char *filename = args->filename;
	char *d_name = args->d_name;

	image im = load_image_color(input,0,0);
	image sized = letterbox_image(im, net->w, net->h);
	layer l = net->layers[net->n-1];

    double time;
    float nms=.45;
	float *X = sized.data;
	time=what_time_is_it_now();
	network_predict(net, X);
	printf("%s: Predicted in %f seconds.\n", input, what_time_is_it_now()-time);
	int nboxes = 0;
	detection *dets = get_network_boxes(net, im.w, im.h, thresh, hier_thresh, 0, 1, &nboxes);
	//printf("%d\n", nboxes);
	//if (nms) do_nms_obj(boxes, probs, l.w*l.h*l.n, l.classes, nms);
	if (nms) do_nms_sort(dets, nboxes, l.classes, nms);
	draw_detections(im, dets, nboxes, thresh, names, alphabet, l.classes);
	free_detections(dets, nboxes);

	if(isdir) {
		char output[256];
		sprintf(output, "%s/yolo/%s", filename, d_name);
		save_image(im, output);
	} else
		save_image(im, "predictions");

	free_image(im);
	free_image(sized);

	return NULL;
}

int main(int argc, char **argv)
{
	float thresh = 0.3;
	int gpu_index;

	if(argc < 4) {
		printf("Argument error! <cfg> <weights> <filename>\n");
		exit(0);
	}

#ifndef GPU
	gpu_index = -1;
#else
	if(gpu_index >= 0){
		cuda_set_device(gpu_index);
	}
#endif

	char *filename = (argc > 3) ? argv[3]: 0;
	printf("CFG file: %s\n", argv[1]);
	printf("Weight file: %s\n", argv[2]);
	printf("File/dir: %s\n", argv[3]);

	test_detector("cfg/coco.data", argv[1], argv[2], filename, thresh, .5, THREAD_COUNT);
}
