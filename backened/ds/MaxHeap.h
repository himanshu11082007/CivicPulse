#pragma once

#include <vector>
#include <stdexcept>
#include <algorithm>
#include "../Complaint.h"

/**
 * Fundamentals of Data Structures: Binary Max-Heap Priority Queue
 * 
 * Provides logarithmic-time complaint insertion and priority-based retrieval.
 * Root (index 0) always contains the highest-priority civic complaint.
 * 
 * Parent: (i - 1) / 2
 * Left child: 2 * i + 1
 * Right child: 2 * i + 2
 */
class MaxHeap {
private:
    std::vector<Complaint> heap;

    void heapifyUp(int index) {
        while (index > 0) {
            int parent = (index - 1) / 2;
            if (heap[index].hasHigherPriorityThan(heap[parent])) {
                std::swap(heap[index], heap[parent]);
                index = parent;
            } else {
                break;
            }
        }
    }

    void heapifyDown(int index) {
        int n = static_cast<int>(heap.size());
        while (true) {
            int highest = index;
            int left = 2 * index + 1;
            int right = 2 * index + 2;

            if (left < n && heap[left].hasHigherPriorityThan(heap[highest])) {
                highest = left;
            }
            if (right < n && heap[right].hasHigherPriorityThan(heap[highest])) {
                highest = right;
            }

            if (highest != index) {
                std::swap(heap[index], heap[highest]);
                index = highest;
            } else {
                break;
            }
        }
    }

public:
    MaxHeap() = default;

    bool empty() const {
        return heap.empty();
    }

    size_t size() const {
        return heap.size();
    }

    void clear() {
        heap.clear();
    }

    // Insert new complaint into heap in O(log n)
    void insert(const Complaint& c) {
        heap.push_back(c);
        heapifyUp(static_cast<int>(heap.size()) - 1);
    }

    // View highest priority complaint in O(1)
    Complaint peekMax() const {
        if (heap.empty()) {
            throw std::runtime_error("Heap is empty");
        }
        return heap.front();
    }

    // Remove and return highest priority complaint in O(log n)
    Complaint extractMax() {
        if (heap.empty()) {
            throw std::runtime_error("Heap is empty");
        }
        Complaint maxItem = heap.front();
        heap.front() = heap.back();
        heap.pop_back();

        if (!heap.empty()) {
            heapifyDown(0);
        }
        return maxItem;
    }

    // Remove complaint by ID
    bool removeById(int id) {
        for (size_t i = 0; i < heap.size(); ++i) {
            if (heap[i].id == id) {
                heap[i] = heap.back();
                heap.pop_back();
                if (i < heap.size()) {
                    heapifyDown(static_cast<int>(i));
                    heapifyUp(static_cast<int>(i));
                }
                return true;
            }
        }
        return false;
    }

    // Update existing complaint or insert if new
    void update(const Complaint& c) {
        for (size_t i = 0; i < heap.size(); ++i) {
            if (heap[i].id == c.id) {
                heap[i] = c;
                heapifyDown(static_cast<int>(i));
                heapifyUp(static_cast<int>(i));
                return;
            }
        }
        insert(c);
    }

    // HeapSort: Extract all elements in descending priority order in O(n log n)
    std::vector<Complaint> getPriorityRankedList() const {
        MaxHeap copy = *this;
        std::vector<Complaint> sorted;
        sorted.reserve(copy.size());

        while (!copy.empty()) {
            sorted.push_back(copy.extractMax());
        }
        return sorted;
    }

    // Raw underlying array for FDS tree visualizer
    const std::vector<Complaint>& getRawHeapArray() const {
        return heap;
    }
};
