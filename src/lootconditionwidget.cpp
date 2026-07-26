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
        "海洋の心", "TNT", "プリズマリンクリスタル",
        "革のチェストプレート", "鉄の剣", "焼き鱈", "焼き鮭",
        "黒曜石", "火打石", "鉄塊", "火打石と打ち金",
        "ファイヤーチャージ", "金塊", "金の剣", "金の斧",
        "金のクワ", "金のシャベル", "金のツルハシ", "金のブーツ",
        "金のチェストプレート", "金のヘルメット", "金のレギンス",
        "きらめくスイカの薄切り", "軽量用感圧板", "金のニンジン",
        "時計", "金ブロック", "鐘",
        "宝の地図", "コンパス", "白紙の地図", "紙", "羽根", "本",
        "ジャガイモ", "青くなったジャガイモ", "ニンジン", "小麦",
        "怪しげなシチュー", "石炭", "カボチャ", "竹",
        "革の帽子", "革のズボン", "革のブーツ",
        "エンチャントの瓶", "ラピスラズリ",
        "パン", "鉄のヘルメット", "生の豚肉", "生の牛肉", "生の羊肉",
        "棒", "粘土玉", "緑色の染料", "サボテン", "生鱈", "生鮭",
        "水入りバケツ", "樽", "小麦の種", "矢", "卵", "植木鉢",
        "石", "石レンガ", "黄色の染料", "滑らかな石", "タンポポ",
        "ポピー", "リンゴ", "オークの苗木", "草", "背の高い草",
        "アカシアの苗木", "松明", "バケツ", "白色の羊毛",
        "黒色の羊毛", "灰色の羊毛", "茶色の羊毛", "薄灰色の羊毛",
        "ハサミ", "青氷", "雪ブロック", "ビートルートの種",
        "ビートルートスープ", "かまど", "雪玉", "シダ", "大きなシダ",
        "スイートベリー", "カボチャの種", "パンプキンパイ",
        "トウヒの苗木", "トウヒの看板", "トウヒの原木", "革",
        "レッドストーンダスト", "鉄のツルハシ", "鉄のシャベル",
        "鉄のチェストプレート", "鉄のレギンス", "鉄のブーツ",
        "ロードストーン", "クロスボウ", "光の矢",
        "きらめくブラックストーン", "泣く黒曜石",
        "ダイヤモンドのシャベル", "ネザライトの欠片", "古代の残骸",
        "グロウストーン", "ソウルサンド", "真紅のナイリウム",
        "焼き豚", "真紅のキノコ", "真紅の根", "ピグリンの旗の模様",
        "レコード（Pigstep）", "鎖", "マグマクリーム", "骨ブロック",
        "ネザライトインゴット", "ダイヤモンドの剣",
        "ダイヤモンドのチェストプレート", "ダイヤモンドのヘルメット",
        "ダイヤモンドのレギンス", "ダイヤモンドのブーツ",
        "ネザークォーツ", "枯れ木",
        "アイテム不問（Lootコンテナ位置・個数のみ）",
    };
    return QString::fromUtf8(japanese[item]) + " (" +
        QString::fromLatin1(structureLootItemName(item)) + ")";
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
        "ソウルスピード",
    };
    return QString::fromUtf8(japanese[enchantment]) + " (" +
        QString::fromLatin1(
            desertPyramidEnchantmentName(enchantment)) + ")";
}

}

class LootRuleRow : public QWidget
{
public:
    explicit LootRuleRow(
        int structureType, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_structureType(structureType)
    {
        QGridLayout *layout = new QGridLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        item = new QComboBox(this);
        updateItems();
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

    void setStructureType(int structureType)
    {
        if (m_structureType == structureType)
            return;
        m_structureType = structureType;
        updateItems();
        updateEnchantmentState();
    }

    void updateItems()
    {
        int oldItem = item->currentData().toInt();
        item->clear();
        for (int i = 0; i < DP_LOOT_ITEM_COUNT; i++)
        {
            if (structureLootItemAvailable(m_structureType, i))
                item->addItem(itemDisplayName(i), i);
        }
        int index = item->findData(oldItem);
        item->setCurrentIndex(index >= 0 ? index : 0);
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
    int m_structureType;
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
    updateChestModes();
    options->addRow(QString::fromUtf8("構造物内の集計"), m_chestMode);
    outer->addLayout(options);

    m_positionPanel = new QWidget(this);
    QVBoxLayout *positionOuter = new QVBoxLayout(m_positionPanel);
    positionOuter->setContentsMargins(0, 0, 0, 0);
    QFormLayout *positionOptions = new QFormLayout();
    m_positionMode = new QComboBox(m_positionPanel);
    updatePositionModes();
    positionOptions->addRow(
        QString::fromUtf8("チェスト座標"), m_positionMode);
    positionOuter->addLayout(positionOptions);

    m_positionRange = new QWidget(m_positionPanel);
    QGridLayout *positionGrid = new QGridLayout(m_positionRange);
    positionGrid->setContentsMargins(0, 0, 0, 0);
    auto setupCoordinate = [](QSpinBox *box, int value)
    {
        box->setRange(-30000000, 30000000);
        box->setValue(value);
    };
    m_chestMinX = new QSpinBox(m_positionRange);
    m_chestMaxX = new QSpinBox(m_positionRange);
    m_chestMinY = new QSpinBox(m_positionRange);
    m_chestMaxY = new QSpinBox(m_positionRange);
    m_chestMinZ = new QSpinBox(m_positionRange);
    m_chestMaxZ = new QSpinBox(m_positionRange);
    setupCoordinate(m_chestMinX, -30000000);
    setupCoordinate(m_chestMaxX, 30000000);
    setupCoordinate(m_chestMinY, -64);
    setupCoordinate(m_chestMaxY, 320);
    setupCoordinate(m_chestMinZ, -30000000);
    setupCoordinate(m_chestMaxZ, 30000000);
    QSpinBox *minimums[] = {
        m_chestMinX, m_chestMinY, m_chestMinZ,
    };
    QSpinBox *maximums[] = {
        m_chestMaxX, m_chestMaxY, m_chestMaxZ,
    };
    const char *axisNames[] = {"X", "Y", "Z"};
    for (int axis = 0; axis < 3; axis++)
    {
        positionGrid->addWidget(
            new QLabel(QString::fromLatin1(axisNames[axis]),
                       m_positionRange),
            axis, 0);
        positionGrid->addWidget(minimums[axis], axis, 1);
        positionGrid->addWidget(new QLabel("～", m_positionRange),
                                axis, 2);
        positionGrid->addWidget(maximums[axis], axis, 3);
    }
    positionOuter->addWidget(m_positionRange);
    m_positionHint = new QLabel(m_positionPanel);
    m_positionHint->setWordWrap(true);
    positionOuter->addWidget(m_positionHint);
    outer->addWidget(m_positionPanel);

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
    connect(m_positionMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this] { updatePositionState(); });
    connect(m_add, &QPushButton::clicked,
            this, [this] { addRule(); });
    addRule();
    m_enabled->setChecked(false);
    updateEnabledState();
}

void LootRuleEditor::setContext(int structureType, int mc)
{
    bool changed = m_structureType != structureType;
    m_structureType = structureType;
    m_mc = mc;
    if (changed)
    {
        for (LootRuleRow *row : m_rows)
            row->setStructureType(structureType);
        updateChestModes();
        updatePositionModes();
    }
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
    setContext(rules.structureType, m_mc);
    while (!m_rows.isEmpty())
        removeRule(m_rows.last());
    m_logic->setCurrentIndex(m_logic->findData(rules.logic));
    m_instanceMode->setCurrentIndex(
        m_instanceMode->findData(rules.instanceMode));
    m_chestMode->setCurrentIndex(
        m_chestMode->findData(rules.chestMode));
    m_positionMode->setCurrentIndex(
        m_positionMode->findData(rules.chestPositionMode));
    m_chestMinX->setValue(rules.chestMinX);
    m_chestMaxX->setValue(rules.chestMaxX);
    m_chestMinY->setValue(rules.chestMinY);
    m_chestMaxY->setValue(rules.chestMaxY);
    m_chestMinZ->setValue(rules.chestMinZ);
    m_chestMaxZ->setValue(rules.chestMaxZ);
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
    rules.chestPositionMode =
        m_positionMode->currentData().toInt();
    rules.chestMinX = m_chestMinX->value();
    rules.chestMaxX = m_chestMaxX->value();
    rules.chestMinY = m_chestMinY->value();
    rules.chestMaxY = m_chestMaxY->value();
    rules.chestMinZ = m_chestMinZ->value();
    rules.chestMaxZ = m_chestMaxZ->value();
    for (LootRuleRow *row : m_rows)
        rules.rules.push_back(row->value());
    return rules;
}

void LootRuleEditor::addRule(const LootRule& rule)
{
    LootRuleRow *row =
        new LootRuleRow(m_structureType, m_rowsWidget);
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

void LootRuleEditor::updateChestModes()
{
    int oldMode = m_chestMode->currentData().toInt();
    m_chestMode->clear();
    m_chestMode->addItem(
        QString::fromUtf8("構造物内の全チェストを合計"),
        LootRuleSet::CHESTS_TOTAL);
    m_chestMode->addItem(
        QString::fromUtf8("いずれか1個のチェストが満たす"),
        LootRuleSet::CHEST_ANY);
    m_chestMode->addItem(
        QString::fromUtf8("各チェストがそれぞれ満たす"),
        LootRuleSet::CHEST_EVERY);

    if (m_structureType == Desert_Pyramid)
    {
        for (int i = 0; i < 4; i++)
        {
            m_chestMode->addItem(
                QString::fromUtf8("生成順チェスト %1").arg(i + 1),
                LootRuleSet::CHEST_1 + i);
        }
    }
    else if (m_structureType == Shipwreck)
    {
        m_chestMode->addItem(
            QString::fromUtf8("物資チェスト"),
            LootRuleSet::CHEST_1);
        m_chestMode->addItem(
            QString::fromUtf8("地図チェスト"),
            LootRuleSet::CHEST_2);
        m_chestMode->addItem(
            QString::fromUtf8("宝物チェスト"),
            LootRuleSet::CHEST_3);
    }
    else if (m_structureType != Bastion &&
             m_structureType != Village)
    {
        m_chestMode->addItem(
            QString::fromUtf8("唯一のチェスト"),
            LootRuleSet::CHEST_1);
    }
    int index = m_chestMode->findData(oldMode);
    m_chestMode->setCurrentIndex(index >= 0 ? index : 0);
}

void LootRuleEditor::updatePositionModes()
{
    int oldMode = m_positionMode->currentData().toInt();
    m_positionMode->clear();
    m_positionMode->addItem(
        QString::fromUtf8("指定しない"),
        LootRuleSet::CHEST_POSITION_ANY);
    m_positionMode->addItem(
        QString::fromUtf8("ワールド絶対座標"),
        LootRuleSet::CHEST_POSITION_ABSOLUTE);
    if (m_structureType == Bastion)
    {
        m_positionMode->addItem(
            QString::fromUtf8("砦の開始チャンク原点との差"),
            LootRuleSet::CHEST_POSITION_RELATIVE);
    }
    int index = m_positionMode->findData(oldMode);
    m_positionMode->setCurrentIndex(index >= 0 ? index : 0);
}

void LootRuleEditor::updatePositionState()
{
    const int mode = m_positionMode->currentData().toInt();
    m_positionRange->setVisible(
        mode != LootRuleSet::CHEST_POSITION_ANY);
    m_positionHint->setVisible(
        mode == LootRuleSet::CHEST_POSITION_RELATIVE);
    m_positionHint->setText(QString::fromUtf8(
        "相対X/Zは砦の開始チャンク原点、相対Yは基準Y=32からの差です。"
        "範囲の両端を含みます。"));
}

void LootRuleEditor::updateEnabledState()
{
    QString unsupported =
        lootSupportDescription(m_structureType, m_mc);
    bool supported = unsupported.isEmpty();
    if (!supported)
    {
        m_support->setText(unsupported);
    }
    else if (m_structureType == Village)
    {
        m_support->setText(QString::fromUtf8(
            "村のピースとLootコンテナ座標は地表高度を含めて計算します。"
            "木・ブロック山・サボテン等が同じチャンクで先行し、"
            "内容乱数を安全に確定できない候補は「未確定」として"
            "最終結果へ混ぜません。位置だけの検索は常に利用できます。"
            "個数範囲は両端を含みます。"));
    }
    else
    {
        m_support->setText(QString::fromUtf8(
            "個数は両端を含みます。最大を「上限なし」にできます。"
            "エンチャントの本を選ぶと種類とレベルも指定できます。"));
    }

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
    const bool supportsPosition =
        m_structureType == Bastion || m_structureType == Village;
    m_positionPanel->setVisible(supportsPosition);
    m_positionMode->setEnabled(active && supportsPosition);
    m_positionRange->setEnabled(
        active &&
        m_positionMode->currentData().toInt() !=
            LootRuleSet::CHEST_POSITION_ANY);
    updatePositionState();
    m_rowsWidget->setEnabled(active);
    m_add->setEnabled(active);
}
