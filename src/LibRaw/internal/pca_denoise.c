#include "svd_arvo.c"

/**
 * converted from loony matlab code accompanying the following paper:
 * Lei Zhang, R. Lukac, X. Wu and D. Zhang, 
 * PCA-based Spatial Adaptive Denoising of CFA Images for Single-Sensor Digital Cameras,
 * IEEE Trans. on Image Processing, 2009.
 *
 * TODO: this is gpu work.
 */
void CLASS pre_interpolate_pca_denoise()
{ // this is going to be so slow. :(
  ushort (*img)[4];
  const int pca_k = 34; // size of the training block
  const int pca_s = 6;  // size of the denoise block
  // const int pca_k2 = pca_k/2;
  const int N = (pca_k-pca_s)/2+1; // 15
  const int D = pca_s*pca_s; // dimension of one vector
  const int L = N*N;  // number of training basis functions, 225 for the settings above
  int row, col, k, i, j, ii, jj, ind, ch;
  const int var[4] = {32, 32, 32, 32}; // noise levels
  // TODO: init matrix D[pca_s,pca_s]: the noise pattern:
  //       entry-wise sigma^2 of respective underlying color channel

  img = (ushort (*)[4]) calloc (height*width, sizeof *image);
  merror (img, "pre_interpolate_pca_denoise()");
  memcpy(img,image,height*width*sizeof *image);

#ifdef _OPENMP
  #pragma omp parallel for default(shared) private(row,col,k,i,j,ii,jj,ind,ch)
#endif
  for (row=0;row<height-pca_k;row+=2)
  {
    printf("[pca denoise] processing row %d\n", row);
    for (col=0;col<width-pca_k;col+=2)
    {
      // 1. get training data
      // get pca_k by pca_k block starting at (col,row), we store the swizzled stuff in X
      float X[L][D];
      float varnx[D];
      float vary, varn, c;
      float Y[L][D];

      ind = 0; // < D
      // for each dimension of one basis function
      for(j=0;j<pca_s;j++) for(i=0;i<pca_s;i++)
      { // store pca_s * pca_s block of cfa data (all 4 channels) at X[ind][..]
        k = 0; // < L
        ch = FC(row+j,col+i);
        // debug: see only one channel
        // if(ch == 1 || ch == 3)
          for(jj=0;jj<N;jj++) for(ii=0;ii<N;ii++) X[k++][ind] = img[width * (row+j+2*jj) + col + i+2*ii][ch];
        // else
          // for(jj=0;jj<N;jj++) for(ii=0;ii<N;ii++) X[k++][ind] = 0.0f;
        varnx[ind] = var[ch]; // remember channel and var for it.
        ind++;
      }

      // compare this to matlab input data:
      // for(k=0;k<L;k++) printf("X %d = %f\n", k, X[k][0]);

      // 2. grouping:
      // FIXME: is this needed at all? /me thinks this is only a hack to speed up matlab.
      // TODO: find num most significant elements in X by simple heuristic on deviation from some center element
      const int num = L;

      // 3. pca transformation:
      // make X[num][D] zero mean
      float Xmean[num] = {0.0};
      for (k=0;k<num;k++) for(j=0;j<D;j++) Xmean[k] += X[k][j];
      for (k=0;k<num;k++) Xmean[k] *= (1.0/D);
      for (k=0;k<num;k++) for(j=0;j<D;j++) X[k][j]  -= Xmean[k];

      // for(k=0;k<D;k++) printf("X mean %d = %f\n", k, Xmean[k]);

      // for(k=0;k<D;k++) printf("X %d = %f\n", k, X[0][k]);

      // covariance matrix = X*X'/(L-1)
      float cov[D][D] = {{0.0}};
      for(k=0;k<D;k++) for(j=0;j<D;j++) for(i=0;i<num;i++) cov[k][j] += X[i][k] * X[i][j] * (1.0/(num-1.0));
      // subtract variances from diagonal:
      for(k=0;k<D;k++) cov[k][k] = fmaxf(0.0001, cov[k][k] - varnx[k]);

      /*for(j=0;j<D;j++)
      {
        for(k=0;k<D;k++) printf("%f ", cov[j][k]);
        printf("\n");
      }*/

      float eval[D];
      float vt[D*D];
      float *a = (float *)cov;
      // pca transform => Y = vt*X (svd)
      // a = usv^t, a[n*m] is replaced by u[n*m], w[n] are the singular values as vect, vt[n*n] = v^t.
      svd(a, eval, vt, D, D);
      // printf("eval:\n");
      // for(k=0;k<D;k++) printf("%d -- %f\n", k, eval[k]);

      //  y = v^t * X
      for(k=0;k<L;k++)
      {
        for(i=0;i<D;i++)
        {
          Y[k][i] = 0.0f;
          for(j=0;j<D;j++) Y[k][i] += vt[D*i+j]*X[k][j];
        }
      }

      // apply bayesian smoothing ( *= c with c = (vy - vn)/vy )
      // for Y[][k] in Y[L][D]
      for(k=0;k<D;k++)
      { // for all dimensions of a basis vector
        // get variance of y: E(Y^2) (zero mean)
        vary = 0.0f;
        for(j=0;j<num;j++) vary += Y[j][k] * Y[j][k];
        vary *= 1.0/num;
        varn = 0.0f;
        // transform var[j] to pca space (including swizzling from L->num decimation)
        for(j=0;j<D;j++) varn += vt[D*j+k] * vt[D*j+k] * varnx[j];
        c = fmaxf(0.0, vary - varn)/(vary + 0.0001f);
        for(j=0;j<num;j++) Y[j][k] *= c;
      }

      // 4. inverse pca transform
      // backtransform B = v * Y + previously subtracted mean.
      for(i=0;i<D;i++)
      {
        X[0][i] = Xmean[i];
        for(j=0;j<D;j++) X[0][i] += vt[D*j+i]*Y[0][j];
      }

      // 5. reshape
      // copy back the very center 2x2 values (1x all colors) of B
      image[width*(row-1+pca_k/2  ) + col-1+pca_k/2+1][FC(row-1+pca_k/2,   col-1+pca_k/2+1)] = X[0][pca_s*2 + 3]; 
      image[width*(row-1+pca_k/2+1) + col-1+pca_k/2+1][FC(row-1+pca_k/2+1, col-1+pca_k/2+1)] = X[0][pca_s*3 + 3]; 
      image[width*(row-1+pca_k/2+1) + col-1+pca_k/2  ][FC(row-1+pca_k/2+1, col-1+pca_k/2)  ] = X[0][pca_s*3 + 2]; 
      image[width*(row-1+pca_k/2  ) + col-1+pca_k/2  ][FC(row-1+pca_k/2,   col-1+pca_k/2)  ] = X[0][pca_s*2 + 2]; 
    }
  }
  free(img);
}

