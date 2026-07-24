#include "lootconditionwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QString itemDisplayName(int item)
{
    static const char *japanese[DP_LOOT_ITEM_COUNT] = {
        "ダイヤモンド", "鉄インゴット", "金インゴット", "エメラルド",
        "骨", "クモの目", "腐った肉", "サドル", "鉄の馬鎧",
        "金の馬鎧", "ダイヤモンドの馬鎧", "エンチャントの本",
        "金のリンゴ", "エンチャントされた金のリンゴ",
        "火薬", "糸", "砂",
    };
    return QString::fromUtf8(japanese[item]) + " (" +
        QString::fromLatin1(desertPyramidLootItemName(item)) + ")";
}

QString enchantmentDisplayName(int enchantment)
{
    static const char *japanese[DP_ENCH_COUNT] = {
        "ダメージ軽減", "火炎耐性", "落下耐性", "爆発耐性",
        "飛び道具耐性", "水中呼吸", "水中採掘", "棘の鎧",
        "水中歩行", "氷渡り", "束縛の呪い", "ダメージ増加",
        "アンデッド特効", "虫特効", "ノックバック", "火属性",
        "ドロップ増加", "範囲ダメージ増加", "効率強化",
        "シルクタッチ", "耐久力", "幸運", "射撃ダメージ増加",
        "パンチ", "フレイム", "無限", "宝釣り", "入れ食い",
        "忠誠", "水生特効", "激流", "召雷", "拡散", "高速装填",
        "貫通", "修繕", "消滅の呪い",
    };
    return QString::fromUtf8(japanese[enchantment]) + " (" +
        QString::fromLatin1(
            desertPyramidEnchantmentName(enchantment)) + ")";
}

}

class LootRuleRow : public QWidget
{
public:
    explicit LootRuleRow(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        QGridLayout *layout = new QGridLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        item = new QComboBox(this);
        for (int i = 0; i < DP_LOOT_ITEM_COUNT; i++)
            item->addItem(itemDisplayName(i), i);
        item->setMinimumContentsLength(16);

        minCount = new QSpinBox(this);
        minCount->setRange(0, 2000000000);
        minCount->setValue(1);
        maxCount = new QSpinBox(this);
        maxCount->setRange(-1, 2000000000);
        maxCount->setSpecialValueText(QString::fromUtf8("上限なし"));
        maxCount->setValue(-1);

        enchantment = new QComboBox(this);
        enchantment->addItem(QString::fromUtf8("種類を指定しない"), -1);
        for (int i = 0; i < DP_ENCH_COUNT; i++)
            enchantment->addItem(enchantmentDisplayName(i), i);

        minLevel = new QSpinBox(this);
        maxLevel = new QSpinBox(this);
        minLevel->setRange(1, DP_ENCH_MAX_LEVEL);
        maxLevel->setRange(1, DP_ENCH_MAX_LEVEL);
        minLevel->setValue(1);
        maxLevel->setValue(DP_ENCH_MAX_LEVEL);

        remove = new QPushButton(QString::fromUtf8("削除"), this);

        layout->addWidget(item, 0, 0, 1, 3);
        layout->addWidget(
            new QLabel(QString::fromUtf8("個数"), this), 0, 3);
        layout->addWidget(minCount, 0, 4);
        layout->addWidget(new QLabel("～", this), 0, 5);
        layout->addWidget(maxCount, 0, 6);
        layout->addWidget(remove, 0, 7);

        layout->addWidget(
            new QLabel(QString::fromUtf8("エンチャント"), this), 1, 0);
        layout->addWidget(enchantment, 1, 1, 1, 3);
        layout->addWidget(
            new QLabel(QString::fromUtf8("Lv."), this), 1, 4);
        layout->addWidget(minLevel, 1, 5);
        layout->addWidget(new QLabel("～", this), 1, 6);
        layout->addWidget(maxLevel, 1, 7);
        layout->setColumnStretch(2, 1);

        connect(item, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this] { updateEnchantmentState(); });
        connect(enchantment, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this] { updateEnchantmentState(); });
        updateEnchantmentState();
    }

    LootRule value() const
    {
        LootRule rule;
        rule.item = item->currentData().toInt();
        rule.minCount = minCount->value();
        rule.maxCount = maxCount->value();
        rule.enchantment = enchantment->currentData().toInt();
        rule.minLevel = minLevel->value();
        rule.maxLevel = maxLevel->value();
        return rule;
    }

    void setValue(const LootRule& rule)
    {
        item->setCurrentIndex(item->findData(rule.item));
        minCount->setValue(rule.minCount);
        maxCount->setValue(rule.maxCount);
        enchantment->setCurrentIndex(
            enchantment->findData(rule.enchantment));
        minLevel->setValue(rule.minLevel);
        maxLevel->setValue(rule.maxLevel);
        updateEnchantmentState();
    }

    void updateEnchantmentState()
    {
        bool book = item->currentData().toInt() ==
            DP_LOOT_ENCHANTED_BOOK;
        enchantment->setEnabled(book);
        int ench = enchantment->currentData().toInt();
        bool levels = book && ench >= 0;
        minLevel->setEnabled(levels);
        maxLevel->setEnabled(levels);
        int maximum = levels
            ? desertPyramidEnchantmentMaxLevel(ench)
            : DP_ENCH_MAX_LEVEL;
        minLevel->setMaximum(maximum);
        maxLevel->setMaximum(maximum);
        if (maxLevel->value() < minLevel->value())
            maxLevel->setValue(minLevel->value());
    }

    QComboBox *item;
    QSpinBox *minCount;
    QSpinBox *maxCount;
    QComboBox *enchantment;
    QSpinBox *minLevel;
    QSpinBox *maxLevel;
    QPushButton *remove;
};

LootRuleEditor::LootRuleEditor(QWidget *parent)
    : QGroupBox(QString::fromUtf8("チェスト内容の条件"), parent)
    , m_structureType(Desert_Pyramid)
    , m_mc(MC_1_16_1)
    , m_areaTotal(false)
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    m_enabled = new QCheckBox(
        QString::fromUtf8("チェスト内容で絞り込む"), this);
    outer->addWidget(m_enabled);

    QFormLayout *options = new QFormLayout();
    m_logic = new QComboBox(this);
    m_logic->addItem(
        QString::fromUtf8("すべて満たす (AND)"),
        LootRuleSet::LOGIC_ALL);
    m_logic->addItem(
        QString::fromUtf8("いずれかを満たす (OR)"),
        LootRuleSet::LOGIC_ANY);
    options->addRow(QString::fromUtf8("条件同士"), m_logic);

    m_instanceMode = new QComboBox(this);
    m_instanceMode->addItem(
        QString::fromUtf8("いずれかの構造物が満たす"),
        LootRuleSet::INSTANCE_ANY);
    m_instanceMode->addItem(
        QString::fromUtf8("各構造物がそれぞれ満たす"),
        LootRuleSet::INSTANCE_EVERY);
    m_instanceMode->addItem(
        QString::fromUtf8("範囲内の全構造物を合計"),
        LootRuleSet::INSTANCE_TOTAL);
    options->addRow(QString::fromUtf8("複数の構造物"), m_instanceMode);

    m_chestMode = new QComboBox(this);
    m_chestMode->addItem(
        QString::fromUtf8("構造物内の全チェストを合計"),
        LootRuleSet::CHESTS_TOTAL);
    m_chestMode->addItem(
        QString::fromUtf8("いずれか1個のチェストが満たす"),
        LootRuleSet::CHEST_ANY);
    m_chestMode->addItem(
        QString::fromUtf8("各チェストがそれぞれ満たす"),
        LootRuleSet::CHEST_EVERY);
    for (int i = 0; i < 4; i++)
    {
        m_chestMode->addItem(
            QString::fromUtf8("生成順チェスト %1").arg(i + 1),
            LootRuleSet::CHEST_1 + i);
    }
    options->addRow(QString::fromUtf8("構造物内の集計"), m_chestMode);
    outer->addLayout(options);

    m_support = new QLabel(this);
    m_support->setWordWrap(true);
    outer->addWidget(m_support);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setMinimumHeight(100);
    scroll->setMaximumHeight(260);
    m_rowsWidget = new QWidget(scroll);
    m_rowsLayout = new QVBoxLayout(m_rowsWidget);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->addStretch();
    scroll->setWidget(m_rowsWidget);
    outer->addWidget(scroll);

    m_add = new QPushButton(QString::fromUtf8("＋ アイテム条件を追加"), this);
    outer->addWidget(m_add, 0, Qt::AlignLeft);

    connect(m_enabled, &QCheckBox::toggled,
            this, [this] { updateEnabledState(); });
    connect(m_instanceMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this] { updateEnabledState(); });
    connect(m_add, &QPushButton::clicked,
            this, [this] { addRule(); });
    addRule();
    m_enabled->setChecked(false);
    updateEnabledState();
}

void LootRuleEditor::setContext(int structureType, int mc)
{
    m_structureType = structureType;
    m_mc = mc;
    updateEnabledState();
}

void LootRuleEditor::setAreaTotalMode(bool areaTotal)
{
    m_areaTotal = areaTotal;
    if (areaTotal)
    {
        m_enabled->setChecked(true);
        m_instanceMode->setCurrentIndex(
            m_instanceMode->findData(LootRuleSet::INSTANCE_TOTAL));
    }
    updateEnabledState();
}

void LootRuleEditor::setRuleSet(
    const LootRuleSet& rules, bool enabled)
{
    while (!m_rows.isEmpty())
        removeRule(m_rows.last());
    m_structureType = rules.structureType;
    m_logic->setCurrentIndex(m_logic->findData(rules.logic));
    m_instanceMode->setCurrentIndex(
        m_instanceMode->findData(rules.instanceMode));
    m_chestMode->setCurrentIndex(
        m_chestMode->findData(rules.chestMode));
    for (const LootRule& rule : rules.rules)
        addRule(rule);
    if (m_rows.isEmpty())
        addRule();
    m_enabled->setChecked(enabled || m_areaTotal);
    updateEnabledState();
}

bool LootRuleEditor::lootEnabled() const
{
    return m_enabled->isChecked() && isLootSupported(
        m_structureType, m_mc);
}

LootRuleSet LootRuleEditor::ruleSet() const
{
    LootRuleSet rules;
    rules.structureType = m_structureType;
    rules.logic = m_logic->currentData().toInt();
    rules.instanceMode = m_areaTotal
        ? LootRuleSet::INSTANCE_TOTAL
        : m_instanceMode->currentData().toInt();
    rules.chestMode = m_chestMode->currentData().toInt();
    for (LootRuleRow *row : m_rows)
        rules.rules.push_back(row->value());
    return rules;
}

void LootRuleEditor::addRule(const LootRule& rule)
{
    LootRuleRow *row = new LootRuleRow(m_rowsWidget);
    row->setValue(rule);
    m_rows.insert(m_rows.size(), row);
    m_rowsLayout->insertWidget(m_rowsLayout->count() - 1, row);
    connect(row->remove, &QPushButton::clicked,
            this, [this, row] { removeRule(row); });
}

void LootRuleEditor::removeRule(LootRuleRow *row)
{
    m_rows.removeOne(row);
    m_rowsLayout->removeWidget(row);
    delete row;
}

void LootRuleEditor::updateEnabledState()
{
    QString unsupported =
        lootSupportDescription(m_structureType, m_mc);
    bool supported = unsupported.isEmpty();
    m_support->setText(supported
        ? QString::fromUtf8(
            "個数は両端を含みます。最大を「上限なし」にできます。"
            "エンチャントの本を選ぶと種類とレベルも指定できます。")
        : unsupported);

    m_enabled->setVisible(!m_areaTotal);
    if (m_areaTotal)
        m_enabled->setChecked(true);
    bool active = supported && (m_areaTotal || m_enabled->isChecked());
    m_logic->setEnabled(active);
    m_instanceMode->setEnabled(active && !m_areaTotal);
    bool total = m_areaTotal ||
        m_instanceMode->currentData().toInt() ==
            LootRuleSet::INSTANCE_TOTAL;
    if (total)
    {
        m_chestMode->setCurrentIndex(
            m_chestMode->findData(LootRuleSet::CHESTS_TOTAL));
    }
    m_chestMode->setEnabled(active && !total);
    m_rowsWidget->setEnabled(active);
    m_add->setEnabled(active);
}
