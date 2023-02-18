// -------------------------------------------------------------------
// Timeout macros 
// -------------------------------------------------------------------
// Intended for short internal timing of some tasks like
// LED blinking, relay contact debounce, etc.
// A 'timer' is simply a 16bit memory variable, and timing intervals
// are restricted to apprx. 30 seconds as we have a
// 15 bit counter for milliseconds, + rollover

#ifndef TimeoutMacros_h
#define TimeoutMacros_h

// Memory buffer variable used for timing operations
// This needs to be signed value!
#if defined(__AVR__)
// AVR specific code here
#define MY_TIMER_MASK 0xFFFF
typedef int mytimer_t;
#elif defined(ESP8266)
// ESP8266 specific code here
#define MY_TIMER_MASK 0xFFFF
typedef int16_t mytimer_t;
#elif defined(ESP32)
// ESP32 specific code here
#define MY_TIMER_MASK 0xFFFF
typedef int16_t mytimer_t;
#endif


// Load timer with timeout value (absolute value from now on)
#define setTimeout(x, t)   x =  mytimer_t(mytimer_t(millis() & MY_TIMER_MASK) + t)

// (Re-)Load timer with timeout value, relative to last timeout
// Use it in continous periodic tasks
#define nextTimeout(x,t)    x = x + t

// Returns true, if timeout is true
// Notice: works only for the next 30 seconds after timeout, beware
// of rollover!
#define checkTimeout(x)   (mytimer_t(mytimer_t(millis() & MY_TIMER_MASK)  - x) >= 0 ? true : false)

// Reset timer to current time.
// Afterwards checkTimeout() will return true.
#define resetTimeout(x)   x =  mytimer_t(millis() & MY_TIMER_MASK)

// Get timer value compared to current time
// Negative values means timeout not reached, 
// positive values telling the latency time.
#define getTimeoutLatency(x) mytimer_t((mytimer_t(millis() & MY_TIMER_MASK) - x) & MY_TIMER_MASK)

// Return the max. achievable timeout value
#define getMaxTimeout() ((size_t)0x7FFF)
// Test whether timeout value is allowed
#define isValidTimeout(x)  (x > getMaxTimeout() ? false : true) 


#endif  // eof
