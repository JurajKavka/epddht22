#include <Arduino.h>

template <typename T> 
class CircularArray {
    private:
        T* _array;
        uint16_t _size;
        uint16_t _idx;   // actual position in array
        uint16_t _length; // can be lower or equal than `_size`
        uint16_t _first(); // can be lower or equal than `_size`
    public:
        CircularArray(T arr[], uint16_t size);
        void print();
        void push(T val);
        uint16_t size();
        T *get(uint16_t i);
        T *first();
        T *last();
};

template <typename T> 
CircularArray<T>::CircularArray(T arr[], uint16_t size){
    _array = arr;
    _size = size;
    _length = 0;
    _idx = -1;
}

template <typename T> 
uint16_t CircularArray<T>::_first(){
    if(_length < _size){
        return 0;
    }
    else{
        return (_idx + 1 % _size);
    }
}

template <typename T> 
void CircularArray<T>::print(){
    uint16_t f = _first();
    for(uint16_t i=f; i<_length + f; i++){
        Serial.print(_array[i % _length]);
        if(i != (_length + f) - 1) Serial.print(", ");
    }
    Serial.println();
}

template <typename T> 
void CircularArray<T>::push(T val){
    if(_length >= _size){
        _idx = (++_idx % _size);
    }
    else {
        _length++;
        _idx++;
    }
    _array[_idx] = val;
}

template <typename T> 
T* CircularArray<T>::first(){
    return &_array[_first()];
}

template <typename T> 
T* CircularArray<T>::last(){
    return &_array[_idx];
}

template <typename T> 
T* CircularArray<T>::get(uint16_t i){
    return &_array[(_first() + i) % _length];
}

template <typename T> 
uint16_t CircularArray<T>::size(){
    return _length;
}
