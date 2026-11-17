#include "prism/usb/gnss_plot.hpp"
#include <stdexcept>
#include <iostream>

int main() {
  using namespace prism::gnss_plot;
  auto check=[](bool valid) { if(!valid) throw std::runtime_error("GNSS parser regression"); };
  for(const char* s : {"", " ", " 1", "1 ", "1x", "nan", "inf", "1e999"})
    check(!number(s));
  check(number("0").value_or(-1)==0);
  check(number("-1.25").value_or(0)==-1.25);
  check(number("1e2").value_or(0)==100);
  check(!number("2",0,1) && !number("-1",0,1));
  check(number("1",0,1).value_or(-1)==1);
  check(integer("12",0,20)==12 && integer("1.5",0,20)==-1);
  check(coordinate("3130.000","N",true).value_or(0)==31.5);
  check(coordinate("12130.000","W",false).value_or(0)==-121.5);
  check(!coordinate("3160.0","N",true) && !coordinate("3130.0","E",true));
  check(utcValid("123456.10") && !utcValid("246000.0"));
  // Keep the public heading API covered in standalone SDK builds as well.
  const std::string heading="#UNIHEADINGA,77,GPS,FINE,2445,183970500,25164,0,18,14;SOL_COMPUTED,L1_FLOAT,28.2565,235.9470,22.2890,0.0000,6.9256,12.3724,\"999\",34,8,8,0,3,00,11,1*01c00b30";
  auto parsed=prism::parseGnssHeading(heading);
  check(parsed&&parsed->valid&&!parsed->fixed&&parsed->heading_deg==235.947);
  check(parsed->pitch_deg==22.289&&parsed->baseline_m==28.2565&&parsed->used_satellites==8);
  check(!prism::parseGnssHeading(heading.substr(0,heading.size()-1)+"1"));
  prism::GnssHeadingTracker tracker;
  prism::GnssObservations batch{1,1000,123,false,{{1,1000,heading}}};
  tracker.apply(batch);check(tracker.value().valid);
  batch.records.clear();batch.device_monotonic_ms=1987;tracker.apply(batch);
  check(tracker.state()==prism::GnssHeadingState::Stale&&!tracker.value().heading_deg);
  batch.gap=true;tracker.apply(batch);check(tracker.state()==prism::GnssHeadingState::Discontinuity);
  std::cout<<"GNSS numeric and heading parser regression passed\n";
}
