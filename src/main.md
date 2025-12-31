**Some C++ reminders of the C++ syntax and concepts used in the code:**
-----

1. **Pointers and Dynamic Allocation**:
```cpp
HomeP1Device* p1Meter = nullptr;  // Pointer declaration, initialized to null
p1Meter = new HomeP1Device(ip);   // Dynamic allocation with 'new'
```

2. **References and Value Parameters**:
```cpp
// Example from timing functions
void updateDisplay() {
    String& timeString;     // Reference (uses &)
    float value = p1Meter->getCurrentExport();  // Value copy
}
```

3. **Struct Definition**:
```cpp
struct Config {
    String wifi_ssid;      // Member variables
    String wifi_password;
    float power_on_threshold;
    // etc...
} config;  // Direct instance creation
```

4. **Null Checks and Short-Circuit Evaluation**:
```cpp
if (!socket1 || !p1Meter) return;  // Short-circuit OR operator
if (newState != currentState && canChangeState(0, newState)) {  // Short-circuit AND
```

5. **Ternary Operator**:
```cpp
// Example from display updates
socket1 ? socket1->getCurrentState() : false;  // Conditional ? true_value : false_value
```

6. **Scope Resolution Operator (::)**:
```cpp
WiFi.status() == WL_CONNECTED  // Class::constant or Class::method
```

7. **String to C-String Conversion**:
```cpp
config.wifi_ssid.c_str()  // Convert String to const char* for C functions
```

8. **Constant Values**:
```cpp
const unsigned long MIN_ON_TIME = 300;  // Type-specific constants
```

9. **Member Function Calls**:
```cpp
sensors.update();         // Direct member function call with .
p1Meter->getCurrentExport();  // Pointer member function call with ->
```

10. **Type Casting and Default Values**:
```cpp
config.min_on_time = doc["min_on_time"] | 300UL;  // UL suffix for unsigned long
```

11. **Array Syntax**:
```cpp
unsigned long lastStateChangeTime[3] = {0, 0, 0};  // Array initialization
```

12. **Prefix vs Postfix Operators**:
```cpp
attempts++;  // Postfix increment
++attempts;  // Prefix increment (slightly more efficient)
```

Would you like me to explain any of these concepts in more detail? Or would you like to see more examples of specific C++ features?
