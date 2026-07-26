#ifndef LOOTCONDITIONWIDGET_H
#define LOOTCONDITIONWIDGET_H

#include "lootcondition.h"

#include <QGroupBox>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QVBoxLayout;
class QWidget;

class LootRuleRow;

class LootRuleEditor : public QGroupBox
{
public:
    explicit LootRuleEditor(QWidget *parent = nullptr);

    void setContext(int structureType, int mc);
    void setAreaTotalMode(bool areaTotal);
    void setRuleSet(const LootRuleSet& rules, bool enabled);

    bool lootEnabled() const;
    LootRuleSet ruleSet() const;

private:
    void addRule(const LootRule& rule = LootRule());
    void removeRule(LootRuleRow *row);
    void updateChestModes();
    void updatePositionModes();
    void updatePositionState();
    void updateEnabledState();

    int m_structureType;
    int m_mc;
    bool m_areaTotal;

    QCheckBox *m_enabled;
    QComboBox *m_logic;
    QComboBox *m_instanceMode;
    QComboBox *m_chestMode;
    QWidget *m_positionPanel;
    QComboBox *m_positionMode;
    QWidget *m_positionRange;
    QSpinBox *m_chestMinX;
    QSpinBox *m_chestMaxX;
    QSpinBox *m_chestMinY;
    QSpinBox *m_chestMaxY;
    QSpinBox *m_chestMinZ;
    QSpinBox *m_chestMaxZ;
    QLabel *m_positionHint;
    QLabel *m_support;
    QWidget *m_rowsWidget;
    QVBoxLayout *m_rowsLayout;
    QPushButton *m_add;
    QVector<LootRuleRow*> m_rows;
};

#endif // LOOTCONDITIONWIDGET_H
