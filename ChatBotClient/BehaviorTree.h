#pragma once
#include "pch.h"
#include "BotUtility.h" // GetRandomDouble

// 봇의 행동 트리 노드가 반환할 상태
enum class NodeStatus
{
    Success,
    Failure
};

// 봇의 모든 행동 트리를 구성할 기본 노드 (Interface)
class Node
{
public:
    virtual ~Node() = default;
    virtual NodeStatus Tick(Bot* bot) = 0;
};

// --- Composite Nodes (자식 노드를 가지는 노드) ---

class CompositeNode : public Node
{
public:
    void AddChild(std::unique_ptr<Node> child)
    {
        m_children.push_back(std::move(child));
    }

protected:
    std::vector<std::unique_ptr<Node>> m_children;
};

/**
 * @brief (Selector 노드 - 'OR' 연산)
 */
class Selector : public CompositeNode
{
public:
    virtual NodeStatus Tick(Bot* bot) override
    {
        for (auto& child : m_children)
        {
            if (child->Tick(bot) == NodeStatus::Success)
            {
                return NodeStatus::Success;
            }
        }
        return NodeStatus::Failure;
    }
};

/**
 * @brief (Sequence 노드 - 'AND' 연산)
 */
class Sequence : public CompositeNode
{
public:
    virtual NodeStatus Tick(Bot* bot) override
    {
        for (auto& child : m_children)
        {
            if (child->Tick(bot) == NodeStatus::Failure)
            {
                return NodeStatus::Failure;
            }
        }
        return NodeStatus::Success;
    }
};

/**
 * @brief (Probabilistic Selector 노드 - '확률적 OR' 연산)
 */
class ProbabilisticSelector : public CompositeNode
{
public:
    // 가중치와 함께 자식 추가
    void AddChild(std::unique_ptr<Node> child, double weight)
    {
        CompositeNode::AddChild(std::move(child));
        m_weights.push_back(weight);
    }

    virtual NodeStatus Tick(Bot* bot) override
    {
        if (m_children.empty())
        {
            return NodeStatus::Success;
        }

        double totalWeight = 0.0;
        for (double w : m_weights)
        {
            totalWeight += w;
        }

        if (totalWeight <= 0.0)
        {
            return NodeStatus::Failure;
        }

        double roll = GetRandomDouble(0.0, totalWeight);

        double currentSum = 0.0;
        for (size_t i = 0; i < m_children.size(); ++i)
        {
            currentSum += m_weights[i];
            if (roll < currentSum)
            {
                return m_children[i]->Tick(bot); // 선택된 자식 실행
            }
        }
        return m_children.back()->Tick(bot);
    }

private:
    std::vector<double> m_weights;
};
