/*************************************************
EECS 220 Final Project
Histogram Equalization using Threads
Name: Rahul Rudradevan
UCI ID: 28688203
Uc Irvine
*************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <tiffio.h>
#include <math.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>

#define MAX_SAT 256
#define MAX_THREADS 128

void do_equalization(void *);

volatile sig_atomic_t cdf_pipe[MAX_THREADS];
volatile sig_atomic_t min_cdf=0;
pthread_mutex_t mutex[MAX_SAT];
pthread_barrier_t barr;

/************************************************

The following structure is passed to each
individual thread

index: the thread index
size: the size of the input
num_threads: number of threads in the system
pixel_value: array that counts pixel intensities
cdf: cummulative distribution of intensities
hist: the final histogram value array
in_image: array input
out_image: array output

***********************************************/

typedef struct str_thdata
{
  int index;
  int size;
  int num_threads;
  int *pixel_value;
  int *cdf;
  int *hist;
  unsigned char *in_image;
  unsigned char *out_image;
} thdata;

int main(int argc, char *argv[])
{
  uint32 r; 						
  uint32 c; 						
  uint32 rows;					
  uint32 columns;					
  uint16 bps;			
  uint16 spp;			
  uint16 pm; 			
  TIFF *in_file_ptr; 				
  TIFF *out_file_ptr;	 			
  int	status;	  
  int size, num_threads;  
  unsigned char *in_image;		
  unsigned char *out_image;  
  int i,j;  
  pthread_t thread[MAX_THREADS];  
  pthread_attr_t thread_attr;  
  thdata data[MAX_THREADS];
  int *pixel_value;
  int *cdf;
  int *hist;
 
  if (argc != 3)  
  {  
    printf("Syntax: ./hist.o [number_of_threads] [filename]\n");    
    exit(1);    
  }  

  num_threads = atoi(argv[1]);
  
  if (num_threads > 128)  
  {  
    printf("Please provide a number less than 128\n");    
    exit(1);
  }

  printf("Number of threads: %d\n",num_threads); 
  
  //Open the input and output files
  in_file_ptr = TIFFOpen(argv[2], "r"); 

  if (in_file_ptr == NULL)
  { 	
    printf ("File open error\n");  
    exit(0);
  }
 
  out_file_ptr = TIFFOpen ("output.tiff", "w");   
  if (out_file_ptr == NULL)	
    printf ("File open error\n");
   
  //Get the input file parameters
  TIFFGetField(in_file_ptr, TIFFTAG_IMAGELENGTH, &rows);  
  TIFFGetField(in_file_ptr, TIFFTAG_IMAGEWIDTH, &columns);  
  TIFFGetField(in_file_ptr, TIFFTAG_BITSPERSAMPLE, &bps);  
  TIFFGetField(in_file_ptr, TIFFTAG_SAMPLESPERPIXEL, &spp);  
  TIFFGetField(in_file_ptr, TIFFTAG_PHOTOMETRIC, &pm);  

  //Set the output file parameters
  TIFFSetField(out_file_ptr, TIFFTAG_IMAGELENGTH, rows);  
  TIFFSetField(out_file_ptr, TIFFTAG_IMAGEWIDTH, columns);  
  TIFFSetField(out_file_ptr, TIFFTAG_BITSPERSAMPLE, bps);  
  TIFFSetField(out_file_ptr, TIFFTAG_SAMPLESPERPIXEL, spp);  
  TIFFSetField(out_file_ptr, TIFFTAG_PLANARCONFIG, 1);  
  TIFFSetField(out_file_ptr, TIFFTAG_PHOTOMETRIC, pm);  

  //Allocate memory for the input and output arrays
  in_image = (unsigned char *) _TIFFmalloc(rows*columns);  
  if (in_image == NULL) 	
    printf ("TIFF malloc error\n");
  
  out_image = (unsigned char *) _TIFFmalloc(rows*columns);  
  if (out_image == NULL)   
    printf ("TIFF malloc error\n");  

  //Read input into the input array
  for (r=0;r<rows;r++)  
  {  
    status = TIFFReadScanline(in_file_ptr, &in_image[r*columns], r, 1);    
    if (status != 1)	  
      printf ("Read Error\n");
  }  
  
  //Allocate and initialize arrays
  size = rows * columns;  
  pixel_value = (int *) malloc ( MAX_SAT * sizeof(int));  
  cdf = (int *) malloc ( MAX_SAT * sizeof(int));  
  hist = (int *) malloc ( MAX_SAT * sizeof(int));

  for(i=0;i<MAX_SAT;i++)  
  {  
    pixel_value[i] = 0;    
    cdf[i] = 0;    
    hist[i] = 0;       
    pthread_mutex_init(&mutex[i], NULL);    
  }     
  

  //Initialize the barrier for sync
  if(pthread_barrier_init(&barr, NULL, num_threads))  
    printf("Barrier Create Error\n");   
  
  //Init thread attributes
  pthread_attr_init(&thread_attr);  
  pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
  
  //Create thread data and start the threads
  for(i=0;i<num_threads;i++)  
  {  
    cdf_pipe[i] = -1;        
    data[i].index = i;    
    data[i].size = size;      
    data[i].pixel_value = pixel_value;    
    data[i].cdf = cdf;    
    data[i].hist = hist;       
    data[i].in_image = in_image;     
    data[i].out_image = out_image;    
    data[i].num_threads = num_threads;    
    pthread_create(&thread[i], &thread_attr, (void *) &do_equalization, (void *) &data[i]);    
  }  
  
  //Wait for threads to complete
  for(i=0;i<num_threads;i++)  
    pthread_join(thread[i], NULL);   
  
  //Write output to the output file
  for (r=0;r<rows;r++)  
  {  
    status = TIFFWriteScanline(out_file_ptr, &out_image[r*columns], r, 1);    
    if (status != 1)	  
      printf ("Write Error\n");
  }  
 
  //Deallocate and close 
  _TIFFfree(in_image);  
  _TIFFfree(out_image);  
  TIFFClose(in_file_ptr);  
  TIFFClose(out_file_ptr);
  
  exit (0);
}

void do_equalization(void *ptr)
{
  thdata *data;            
  data = (thdata *) ptr; 
  int thread_index = data->index;
  int size = data->size;
  int num_threads = data->num_threads;
  int *pixel_value = data->pixel_value;
  int *cdf = data->cdf;
  int *hist = data->hist;
  unsigned char *in_image = data->in_image;
  unsigned char *out_image = data->out_image;  
  int img_split_size = size/num_threads;
  int img_start = thread_index * img_split_size;
  int img_end = img_start + img_split_size;
  int max_sat_split_size;
  int max_sat_start;
  int max_sat_end; 
  int sum =0;
  int i,j,rc;
  float val;  

  if (img_end > size)
    img_end = size;

   
  //Increment pixel values only if no other
  //thread is already doing so

  for(i=img_start;i<img_end;i++)
  {  
    pthread_mutex_lock(&mutex[in_image[i]]);
    pixel_value[in_image[i]] = pixel_value[in_image[i]] +  1;
    pthread_mutex_unlock(&mutex[in_image[i]]);
  }
  
  //Wait for other threads
  rc = pthread_barrier_wait(&barr);

  if (thread_index == 0)
    cdf_pipe[thread_index] = 0;  

  //The following lines perform CDF

  max_sat_split_size = MAX_SAT/num_threads;

  max_sat_start = thread_index * max_sat_split_size;
  max_sat_end = max_sat_start + max_sat_split_size;

  if (max_sat_end > MAX_SAT)
    max_sat_end = MAX_SAT;

  while(cdf_pipe[thread_index] == -1);

  sum = cdf_pipe[thread_index];
  
  //Calulate the CDF
  for(i=max_sat_start;i<max_sat_end;i++)
  {
    if(pixel_value[i] != 0)
    {
      if(min_cdf == 0)
        min_cdf = pixel_value[i];
      sum = sum + pixel_value[i];
      cdf[i] = sum;
    }
  }
    
  //Cascade the sum to the next thread
  //through the cdf_pipe

  if (thread_index != (num_threads - 1))
    cdf_pipe[thread_index + 1] = sum;

  //Wait for other threads
  rc = pthread_barrier_wait(&barr); 

  //The following lines calculate the histogram value

  for(i=max_sat_start;i<max_sat_end;i++)
  {
    if (cdf[i] != 0)
    {      
      val = ((float)(cdf[i] - min_cdf) / (float)(size - min_cdf)) * (MAX_SAT - 1);
      hist[i] = (int) roundf(val);
    }
  }
  
  //Wait for other threads
  rc = pthread_barrier_wait(&barr);
 
  //Write the final output array
   
  for(i=img_start;i<img_end;i++)
     out_image[i] = hist[in_image[i]];    
  
  pthread_exit(0);
}



