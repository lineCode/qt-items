#ifndef QI_CACHE_SPACE_H
#define QI_CACHE_SPACE_H

#include "space/Space.h"

namespace Qi
{

class ControllerContext;
class CacheItem;
class CacheItemFactory;

class QI_EXPORT CacheSpace: public QObject
{
    friend class CacheControllersMouse;

    Q_OBJECT
    Q_DISABLE_COPY(CacheSpace)

public:
    ~CacheSpace();

    const Space& space() const { return *m_space; }
    Space& rSpace() const { return *m_space; }

    const CacheItemFactory& cacheItemFactory() const { return *m_cacheItemsFactory; }

    ViewApplicationMask viewApplicationMask() const { return m_viewApplicationMask; }
    void setViewApplicationMask(ViewApplicationMask viewApplicationMask);

    const QRect& window() const { return m_window; }
    void setWindow(const QRect& window);

    const QPoint& scrollOffset() const { return m_scrollOffset; }
    void setScrollOffset(const QPoint& scrollOffset);

    QPoint originPos() const { return m_window.topLeft() - m_scrollOffset; }

    void set(const QRect& window, const QPoint& scrollOffset);

    QPoint window2Space(const QPoint& windowPoint) const;
    QPoint space2Window(const QPoint& spacePoint) const;

    void clear();
    const CacheItem* cacheItem(const ItemID& visibleItem) const;
    const CacheItem* cacheItemByPosition(const QPoint& point) const;

    void validate(const GuiContext& ctx) const;
    void draw(QPainter* painter, const GuiContext& ctx) const;
    void drawRaw(QPainter* painter, const GuiContext& ctx) const;

    bool hasDrawProxy() const { return (bool)m_drawProxy; }
    void setDrawProxy(const std::function<void(const CacheSpace*, QPainter* painter, const GuiContext& ctx)>& drawProxy);

    void tryActivateControllers(const ControllerContext& context, QVector<ControllerMouse*>& controllers) const;
    bool tooltipByPoint(const QPoint& point, TooltipInfo& tooltipInfo) const;

    struct QI_EXPORT IterateInfo
    {
        QSharedPointer<CacheItem> cacheItem;
        int cacheItemIndex;
        CacheView* cacheView;
        int cacheViewIndex;

        IterateInfo(): cacheItemIndex(0), cacheView(nullptr), cacheViewIndex(0) {}
    };

    bool forEachCacheItem(const std::function<bool(const QSharedPointer<CacheItem>&)>& visitor) const;
    bool forEachCacheView(const std::function<bool(const IterateInfo&)>& visitor) const;
    //bool forEachCacheView(const std::function<bool(const QSharedPointer<CacheItem>&, CacheView*)>& visitor);

signals:
    void cacheChanged(const CacheSpace* cache, ChangeReason reason);

protected:
    explicit CacheSpace(const QSharedPointer<Space>& space, ViewApplicationMask viewApplicationMask = ViewApplicationDraw);

    void validateItemsCache() const;
    void clearItemsCache() const;
    QSharedPointer<CacheItem> createCacheItem(const ItemID& visibleItem) const;

    virtual void clearItemsCacheImpl() const = 0;
    virtual void validateItemsCacheImpl() const = 0;
    virtual bool forEachCacheItemImpl(const std::function<bool(const QSharedPointer<CacheItem>&)>& visitor) const = 0;
    virtual const CacheItem* cacheItemImpl(const ItemID& visibleItem) const = 0;
    virtual const CacheItem* cacheItemByPositionImpl(const QPoint& point) const = 0;

    // space
    QSharedPointer<Space> m_space;

    ViewApplicationMask m_viewApplicationMask;

    // cache items factory
    QSharedPointer<CacheItemFactory> m_cacheItemsFactory;

    // visible frame
    QRect m_window;
    // offset within frame
    QPoint m_scrollOffset;

    // offset delta between two ValidateItemsCache calls
    mutable QPoint m_scrollDelta;
    mutable QSize m_sizeDelta;
    // items cache validation flag
    mutable bool m_itemsCacheInvalid;

    // flag for debugging
    mutable bool m_cacheIsInUse;

    // proxy function
    std::function<void(const CacheSpace*, QPainter* painter, const GuiContext& ctx)> m_drawProxy;

private:
    void invalidateItemsCache(ChangeReason reason);

    void onSpaceChanged(const Space* space, ChangeReason reason);
    void updateCacheItemsFactory();
};

} // end namespace Qi 

#endif // QI_CACHE_SPACE_H
