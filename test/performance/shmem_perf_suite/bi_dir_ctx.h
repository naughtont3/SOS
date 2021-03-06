/*
*  Copyright (c) 2018 Intel Corporation. All rights reserved.
*  This software is available to you under the BSD license below:
*
*      Redistribution and use in source and binary forms, with or
*      without modification, are permitted provided that the following
*      conditions are met:
*
*      - Redistributions of source code must retain the above
*        copyright notice, this list of conditions and the following
*        disclaimer.
*
*      - Redistributions in binary form must reproduce the above
*        copyright notice, this list of conditions and the following
*        disclaimer in the documentation and/or other materials
*        provided with the distribution.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
* BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
* ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


static inline void bi_bw_ctx (int len, perf_metrics_t *metric_info)
{
    double start = 0.0, end = 0.0;
    int dest = partner_node(*metric_info);
    unsigned long int i, j;
    char *src = aligned_buffer_alloc(metric_info->nthreads * len);
    char *dst = aligned_buffer_alloc(metric_info->nthreads * len);
    assert(src && dst);
    static int check_once = 0;

    if (!check_once) {
        /* check to see whether sender and receiver are the same process */
        if (dest == metric_info->my_node) {
            fprintf(stderr, "Warning: Sender and receiver are the same process (%d)\n", 
                             dest);
        }
        /* hostname validation for all sender and receiver processes */
        int status = check_hostname_validation(*metric_info);
        if (status != 0) return;
        check_once++;
    }

    shmem_barrier_all();

#pragma omp parallel default(none) firstprivate(len, dest) private(i, j) \
    shared(metric_info, src, dst, start, end) num_threads(metric_info->nthreads)
    {
        const int thread_id = omp_get_thread_num();
        shmem_ctx_t ctx;
        shmem_ctx_create(SHMEM_CTX_PRIVATE, &ctx);

        for (i = 0; i < metric_info->warmup; i++) {
            for(j = 0; j < metric_info->window_size; j++) {
#ifdef USE_NONBLOCKING_API
                shmem_ctx_putmem_nbi(ctx, dst + thread_id * len, src + thread_id * len, len, dest);
#else
                shmem_ctx_putmem(ctx, dst + thread_id * len, src + thread_id * len, len, dest);
#endif
            }
            shmem_ctx_quiet(ctx);
        }
        shmem_ctx_destroy(ctx);
    }

    shmem_barrier_all();
    if (streaming_node(*metric_info)) {
#pragma omp parallel default(none) firstprivate(len, dest) private(i, j) \
        shared(metric_info, src, dst, start, end) num_threads(metric_info->nthreads)
        {
            const int thread_id = omp_get_thread_num();
            shmem_ctx_t ctx;
            shmem_ctx_create(SHMEM_CTX_PRIVATE, &ctx);

#pragma omp barrier
#pragma omp master
            {
                start = perf_shmemx_wtime();
            }
            for (i = 0; i < metric_info->trials; i++) {
                for(j = 0; j < metric_info->window_size; j++) {
#ifdef USE_NONBLOCKING_API
                    shmem_ctx_putmem_nbi(ctx, dst + thread_id * len, src + thread_id * len, len, dest);
#else
                    shmem_ctx_putmem(ctx, dst + thread_id * len, src + thread_id * len, len, dest);
#endif
                }
                shmem_ctx_quiet(ctx);
            }
            shmem_ctx_destroy(ctx);
        }
    } else {
#pragma omp parallel default(none) firstprivate(len, dest) private(i, j) \
        shared(metric_info, src, dst, start, end) num_threads(metric_info->nthreads)
        {
            const int thread_id = omp_get_thread_num();
            shmem_ctx_t ctx;
            shmem_ctx_create(SHMEM_CTX_PRIVATE, &ctx);

            for (i = 0; i < metric_info->trials; i++) {
                for(j = 0; j < metric_info->window_size; j++) {
#ifdef USE_NONBLOCKING_API
                    shmem_ctx_putmem_nbi(ctx, dst + thread_id * len, src + thread_id * len, len, dest);
#else
                    shmem_ctx_putmem(ctx, dst + thread_id * len, src + thread_id * len, len, dest);
#endif
                }
                shmem_ctx_quiet(ctx);
            }
            shmem_ctx_destroy(ctx);
        }
    }

    shmem_barrier_all();
    if (streaming_node(*metric_info)) {
        end = perf_shmemx_wtime();
        calc_and_print_results(end, start, len, *metric_info);
    }

    shmem_barrier_all();

    aligned_buffer_free(src);
    aligned_buffer_free(dst);

}
