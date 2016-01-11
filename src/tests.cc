#include "common.h"
#include "dsp.h"

#include <map>
#include <cmath>

void printWave (Wave wave, double dt) {
  for (int i=0;i<(int)wave.size();i++)
    printf("%lf,%lf\n",i*dt,wave[i]);
}

double sum (std::vector<double> nums) {
  double result = 0;
  for (double d : nums)
    result += d;
  return result;
}

double mean (std::vector<double> nums) {
  return sum(nums) / nums.size();
}

double sdev (std::vector<double> nums) {
  double result = 0;
  double _mean = mean(nums);
  for (double d : nums)
    result += std::pow(d - _mean, 2);
  result = std::sqrt(result / nums.size());
  return result;
}

void runTest(const char *sweepsound) {
  DSP dsp;
  double tone_len = 0.3;
  dsp.getInput()->openAudioFile(sweepsound);
  std::map<double, std::vector<double> > errors;

  while(dsp.getInput()->isOpen()) {
    double f = dsp.calcPeakFreq(1200,2500,WINDOW_HANN63);
    double lum_should_be = int(dsp.getInput()->get_t()/tone_len) % 256;
    double f_should_be = 1500 + lum_should_be / 255.0 * (2300-1500);

    printf("at t=%f, f should be %.0f, is %.0f (error %.1f)\n",dsp.getInput()->get_t(),f_should_be,f,f - f_should_be);

    errors[f_should_be].push_back((f - f_should_be) );

    dsp.getInput()->forward();
  }

  typedef std::map<double, std::vector<double> >::iterator it_type;
  for(it_type iterator = errors.begin(); iterator != errors.end(); iterator++) {
    double k = iterator->first;
    std::vector<double> errs = iterator->second;
    printf("%f,%f\n",k,sdev(errs));
  }
}
