#pragma once
#include "pch.h"
#include "BotUtility.h"

enum class NodeStatus
{
    Success,
    Failure
};

class Node
{
public:
    virtual ~Node() = default;
    virtual NodeStatus Tick(Bot* bot) = 0;
};

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

// Selector node handles 'OR' logic
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

// Sequence node handles 'AND' logic
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

// Probabilistic Selector node handles 'probabilistic OR' logic
class ProbabilisticSelector : public CompositeNode
{
public:
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
                return m_children[i]->Tick(bot);
            }
        }
        return m_children.back()->Tick(bot);
    }

private:
    std::vector<double> m_weights;
};
