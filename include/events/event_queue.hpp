#pragma once

#include "event.hpp"

#include <queue>
#include <memory>

namespace engine::events
{
    /**
     * @brief A FIFO queue for managing event objects in the simulation.
     */
    class event_queue
    {
    public:
        /**
         * @brief Push a new event onto the queue.
         * @param ev A unique pointer to an event object.
         */
        void push(event ev);

        /**
         * @brief Pop the next event from the queue.
         * @return A unique pointer to the next event.
         * @note Assumes queue is not empty; call `empty()` before popping.
         */
        event pop();

        /**
         * @brief Check whether the queue is empty.
         * @return True if empty, false otherwise.
         */
        bool empty() const;

    private:
        std::queue<event> queue_;
    };
} // namespace engine::events
