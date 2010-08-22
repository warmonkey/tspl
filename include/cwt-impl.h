/*****************************************************************************
 *                               cwt-impl.h
 *
 * Implementation for CWT class.
 *
 * Zhang Ming, 2010-03, Xi'an Jiaotong University.
 *****************************************************************************/


/**
 * constructors and destructor
 */
template<typename Type>
CWT<Type>::CWT( const string &name ) : waveType(name)
{
    if( (waveType != "mexiHat") && (waveType != "morlet") )
    {
        cerr << "No such wavelet type!" << endl;
        exit(1);
    }
}

template<typename Type>
CWT<Type>::~CWT()
{
}


/**
 * Set the scales on which the CWT is computed. The frequency of wavelet
 * family generated from the mather wavelet should cover the frequency
 * band of signal for better reconstruction.
 */
template<typename Type>
void CWT<Type>::setScales( Type fs, Type fmin, Type fmax, Type dj )
{
    double  flc = 0,
            fuc = 0,
            a = pow( Type(2), dj );

    if( waveType == "mexiHat" )
    {
        flc = 0.01;
        fuc = 0.7;
    }
    else if( waveType == "morlet" )
    {
        flc = 0.4;
        fuc = 1.2;
    }

	int jMin = int( ceil( log(fs*flc/fmax)/log(2.0) / dj ) ),
        jMax = int( ceil( log(fs*fuc/fmin)/log(2.0) / dj ) ),
        J = jMax-jMin+1;

    scales.resize(J);
	for( int j=0; j<J; ++j )
		scales[j] = pow( a, double(j+jMin) );

}


/**
 * Generate the table use in forward and backward transform. Each row of
 * the table consist of the N-points frequency sampling values' conjugation
 * of the mather wavelet scaled by "scales[j]". In order to normalization,
 * these values are multiplied by a constant.
 */
template <typename Type>
void CWT<Type>::setTable( int N )
{
    int J = scales.size();
    Vector<Type> omega(N),
                 tmp(N);
    table.resize( J, N );

    for( int j=0; j<J; ++j )
    {
        // fundamental frequency
        Type c = Type( 2*PI*scales[j]/N );
        for( int i=0; i<N; ++i )
            if( i <= N/2 )
                omega[i] = c*i;
            else
                omega[i] = c*(i-N);

        // independent variables of exponential function
        if( waveType == "mexiHat" )
        {
            omega *= omega;
            tmp = Type( sqrt(c) ) * ( omega * exp(Type(-0.5)*omega) );
        }
        else if( waveType == "morlet" )
        {
            Type sigma = 6.2;
            omega = Type(-0.5) * ( (omega-sigma)*(omega-sigma) );
            tmp = Type( sqrt(c) ) * exp(omega);
        }
        else
        {
            cerr << "No such wavelet type!" << endl;
            exit(1);
        }
        table.setRow( tmp, j+1 );
    }

    delta = constDelta();
}


/**
 * Compute the delta constant, which comes from the substitution
 * of the reconstruction function by the delta  function.
 */
template <typename Type>
Type CWT<Type>::constDelta()
{
    int J = scales.size(),
        N = table.cols();
    Type sum;
    Type C = 0;

    for( int j=0; j<J; ++j )
    {
        sum = 0;
        for( int k=0; k<N; ++k )
            sum += table[j][k];
        C += Type( sum / sqrt(scales[j]) );
    }
    return C;
}


/**
 * Compute the continuous wavelet transform of complex mather wavelet.
 * This is a fast algorithm throuth using FFT by the convolution theorem.
 */
template <typename Type>
Matrix< complex<Type> > CWT<Type>::cwtC( Vector<Type> &signal )
{
    int N = signal.size(),
        J = scales.size();

    // initialize the coefficients
    Matrix< complex<Type> > coefs(J, N);
    if( (table.cols() != N) || (table.rows() != J) )
        setTable(N);

    // compute the DFT of input signal
    Vector< complex<Type> > sigDFT(N);
    fft( signal, sigDFT );
    for( int i=N/2+1; i<N; ++i )
        sigDFT[i] = conj(sigDFT[N-i]);

    Vector< complex<Type> > tmp(N);
    Vector< complex<Type> > tmpDFT(N);

    for( int j=0; j<J; ++j )
    {
        // compute the DFT of CWT coefficients at scales[j]
        for( int k=0; k<N; ++k )
            tmpDFT[k] = sigDFT[k]*table[j][k];

        ifft( tmpDFT, tmp );
        coefs.setRow( tmp, j+1 );
    }

    return coefs;
}


/**
 * Compute the inverse CWT of complex mather wavelet. The redundancy of
 * CWT makes it possible to reconstruct the signal using a diffient wavelet,
 * the easiest of which is the delta function. In this case, the
 * reconstructed signal is just the sum of the real part of the wavelet
 * transform over all scales.
 */
template <typename Type>
Vector<Type> CWT<Type>::icwtC( const Matrix< complex<Type> > &coefs )
{
    int J = coefs.rows(),
        N = coefs.cols();
    Vector<Type> signal(N);

    // recover "signal"
    for( int i=0; i<N; ++i )
        for( int j=0; j<J; ++j )
            signal[i] += real(coefs[j][i]) / Type(sqrt(scales[j]));

    signal = signal*Type(N) / delta;
    return signal;
}


/**
 * Calculate the continuous wavelet transform of real mather wavelet.
 */
template <typename Type>
Matrix<Type> CWT<Type>::cwtR( Vector<Type> &signal )
{
    int N = signal.size(),
        J = scales.size();

    // initialize the coefficients
    Matrix<Type> coefs(J, N);
    if( (table.cols() != N) || (table.rows() != J) )
        setTable(N);

    // compute the DFT of input signal
    Vector< complex<Type> > sigDFT(N/2+1);
    fft( signal, sigDFT );

    Vector<Type> tmp(N);
    Vector< complex<Type> > tmpDFT(N/2+1);

    for( int j=0; j<J; ++j )
    {
        // compute the DFT of CWT coefficients at scales[j]
        for( int k=0; k<N/2+1; ++k )
            tmpDFT[k] = sigDFT[k]*table[j][k];

        ifft( tmpDFT, tmp );
        coefs.setRow( tmp, j+1 );
    }

    return coefs;
}


/**
 * Calculate the ICWT of real mather wavelet.
 */
template <typename Type>
Vector<Type> CWT<Type>::icwtR( const Matrix<Type> &coefs )
{
    int J = coefs.rows(),
        N = coefs.cols();
    Vector<Type> signal(N);

    // recover "signal"
    for( int i=0; i<N; ++i )
        for( int j=0; j<J; ++j )
            signal[i] += coefs[j][i] / Type(sqrt(scales[j]));

    signal = signal*Type(N) / delta;
    return signal;
}