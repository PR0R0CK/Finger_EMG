% [Y, FS]=audioread(FILENAME, [START END], DATATYPE);
function [x] = wav_emg(filename)

filename = 'sample/50hz.wav';
filename = 'sample/noise.wav';
filename = 'sample/hand.wav';

[Y, FS]=audioread(filename,'double');
info = audioinfo(filename)
% 
[data, fs] = audioread(filename);
d(info.SampleRate) = data(info.SampleRate);
for i = 1:info.SampleRate
    d(i) = data(i);
end
% d
 data_fft = fft(data) % data
 plot(abs(data_fft(:,1)))
 return;
[y,fs] = audioread(filename);

 whos y
ydft = fft(y);
% I'll assume y has even length
ydft = ydft(1:length(y)/2+1);
% create a frequency vector
freq = 0:fs/length(y):fs/2;
% plot magnitude
subplot(211);
plot(freq,abs(ydft));
% plot phase
subplot(212);
plot(freq,unwrap(angle(ydft))); 
xlabel('Hz');

return;
% below from csv-------------------------------------------------------------------------

Fs=FS;
samples = [1,2*Fs];
[Y, FS] =  audioinfo(filename, samples)

n = info.Duration
whos Y

t = 0:seconds(1/Fs):seconds(info.Duration);
t = t(1:info.TotalSamples);
size(t)
size(Y)
plot(t,Y)
xlabel('Time')
ylabel('Audio Signal')

L = length(Y)-1; 

T = 1/FS; 
t = (0:L-1)*T;                                      % Time vector
Fn = FS/2;                                          % Nyquist Frequency
Fv = linspace(0, 1, fix(L/2)+1)*Fn;                 % Frequency Vector
Iv = 1:size(Fv,2);
%     
%  for i = 1:1
    
%     X=Y(i,:); 
X = Y;
    FX = fft(X)/L;                                      % Fourier Transform
    
%     plot(Fv, abs(FX(Iv))*2); figure(1); grid; 
% end
end