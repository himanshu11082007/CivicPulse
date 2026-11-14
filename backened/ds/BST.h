#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include "../Complaint.h"

/**
 * Fundamentals of Data Structures: Binary Search Tree (BST)
 * 
 * Provides O(log n) expected search, insertion, and ordered traversal
 * of complaints indexed by unique Complaint ID.
 */
class BST {
public:
    struct Node {
        Complaint data;
        Node* left = nullptr;
        Node* right = nullptr;

        Node(const Complaint& c) : data(c), left(nullptr), right(nullptr) {}
    };

private:
    Node* root = nullptr;
    size_t count = 0;

    Node* insertRec(Node* node, const Complaint& c) {
        if (!node) {
            count++;
            return new Node(c);
        }
        if (c.id < node->data.id) {
            node->left = insertRec(node->left, c);
        } else if (c.id > node->data.id) {
            node->right = insertRec(node->right, c);
        } else {
            // Replace existing complaint data
            node->data = c;
        }
        return node;
    }

    Node* findMin(Node* node) const {
        while (node && node->left) {
            node = node->left;
        }
        return node;
    }

    Node* removeRec(Node* node, int id, bool& removed) {
        if (!node) return nullptr;

        if (id < node->data.id) {
            node->left = removeRec(node->left, id, removed);
        } else if (id > node->data.id) {
            node->right = removeRec(node->right, id, removed);
        } else {
            // Found node to delete
            removed = true;
            count--;

            // Case 1 & 2: 0 or 1 child
            if (!node->left) {
                Node* rightChild = node->right;
                delete node;
                return rightChild;
            } else if (!node->right) {
                Node* leftChild = node->left;
                delete node;
                return leftChild;
            }

            // Case 3: 2 children - get inorder successor
            Node* successor = findMin(node->right);
            node->data = successor->data;
            bool dummy = false;
            node->right = removeRec(node->right, successor->data.id, dummy);
        }
        return node;
    }

    void inOrderRec(Node* node, std::vector<Complaint>& result) const {
        if (!node) return;
        inOrderRec(node->left, result);
        result.push_back(node->data);
        inOrderRec(node->right, result);
    }

    void clearRec(Node* node) {
        if (!node) return;
        clearRec(node->left);
        clearRec(node->right);
        delete node;
    }

    int heightRec(Node* node) const {
        if (!node) return 0;
        return 1 + std::max(heightRec(node->left), heightRec(node->right));
    }

public:
    BST() : root(nullptr), count(0) {}

    ~BST() {
        clear();
    }

    // Disable copy constructor and assignment for safe memory handling
    BST(const BST& other) = delete;
    BST& operator=(const BST& other) = delete;

    // Move constructor
    BST(BST&& other) noexcept : root(other.root), count(other.count) {
        other.root = nullptr;
        other.count = 0;
    }

    void insert(const Complaint& c) {
        root = insertRec(root, c);
    }

    // Search complaint by ID, counting comparison steps for academic demonstration
    const Complaint* search(int id, int& comparisonSteps) const {
        comparisonSteps = 0;
        Node* curr = root;
        while (curr) {
            comparisonSteps++;
            if (id == curr->data.id) {
                return &(curr->data);
            } else if (id < curr->data.id) {
                curr = curr->left;
            } else {
                curr = curr->right;
            }
        }
        return nullptr;
    }

    bool remove(int id) {
        bool removed = false;
        root = removeRec(root, id, removed);
        return removed;
    }

    std::vector<Complaint> inOrder() const {
        std::vector<Complaint> result;
        result.reserve(count);
        inOrderRec(root, result);
        return result;
    }

    int height() const {
        return heightRec(root);
    }

    size_t size() const {
        return count;
    }

    bool empty() const {
        return count == 0;
    }

    void clear() {
        clearRec(root);
        root = nullptr;
        count = 0;
    }
};
